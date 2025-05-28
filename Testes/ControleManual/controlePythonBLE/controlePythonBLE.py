import customtkinter as ctk
import asyncio
from bleak import BleakClient, BleakScanner
import platform

# --- Configurações e Variáveis Globais ---
ESP32_NAME = "Hermes_BLE"
SERVICE_UUID = "e5a220a3-ffd5-42a8-ac9c-4cdc31f68e6b"
CHARACTERISTIC_UUID_RX = "beb5483e-36e1-4688-b7f5-ea07361b26a8"

client_ble = None
is_connected = False
send_task = None
is_programmatic_update = False


# --- Funções BLE Assíncronas (MANTIDAS IGUAIS) ---
# ... (discover_and_connect, disconnect_ble, send_ble_data) ...
async def discover_and_connect():
    global client_ble, is_connected
    status_label.configure(text="Procurando ESP32...")
    app.update_idletasks()
    device = None
    try:
        if platform.system() != "Darwin":
            devices = await BleakScanner.discover(timeout=7.0)
            for d in devices:
                if d.name == ESP32_NAME:
                    device = d
                    break
        else:
            device = await BleakScanner.find_device_by_name(ESP32_NAME, timeout=10.0)

        if not device:
            status_label.configure(text=f"Não encontrado por nome. Procurando por serviço...")
            app.update_idletasks()
            device = await BleakScanner.find_device_by_filter(
                lambda d, ad: SERVICE_UUID.lower() in [s.lower() for s in ad.service_uuids],
                timeout=10.0
            )
        if not device:
            status_label.configure(text="ESP32 não encontrado.")
            log_message("ESP32 não encontrado após escaneamento.")
            connect_button.configure(state="normal")
            return
        log_message(f"ESP32 encontrado: {device.name} ({device.address})")
        status_label.configure(text=f"Conectando a {device.name}...")
        app.update_idletasks()
        client_ble = BleakClient(device.address)
        await client_ble.connect()
        if client_ble.is_connected:
            is_connected = True
            status_label.configure(text=f"Conectado a {ESP32_NAME}")
            connect_button.configure(text="Desconectar", command=disconnect_ble_thread, state="normal")
            enable_controls(True) # Habilita controles principais
            update_send_button_state() # Habilita/desabilita botão de envio manual conforme o modo
            log_message("Conectado com sucesso!")
        else:
            status_label.configure(text="Falha ao conectar.")
            log_message("Falha ao conectar.")
            connect_button.configure(state="normal")
    except Exception as e:
        status_label.configure(text=f"Erro: {str(e).splitlines()[0]}")
        log_message(f"Erro durante conexão: {e}")
        is_connected = False
        if client_ble and client_ble.is_connected:
            await client_ble.disconnect()
        client_ble = None
        connect_button.configure(state="normal")

async def disconnect_ble():
    global client_ble, is_connected
    if client_ble and client_ble.is_connected:
        status_label.configure(text="Desconectando...")
        app.update_idletasks()
        await client_ble.disconnect()
    client_ble = None
    is_connected = False
    status_label.configure(text="Desconectado")
    connect_button.configure(text="Conectar", command=connect_ble_thread, state="normal")
    enable_controls(False) # Desabilita controles principais
    send_button.configure(state="disabled") # Desabilita botão de envio manual
    log_message("Desconectado.")

async def send_ble_data(data_str):
    if client_ble and client_ble.is_connected:
        try:
            await client_ble.write_gatt_char(CHARACTERISTIC_UUID_RX, data_str.encode('utf-8'), response=False)
        except Exception as e:
            log_message(f"Erro ao enviar dados: {e}")
            status_label.configure(text="Erro ao enviar dados.")


# --- Funções da GUI e Threads ---
def connect_ble_thread():
    # ... (mantida)
    connect_button.configure(state="disabled")
    asyncio.run_coroutine_threadsafe(discover_and_connect(), loop)

def disconnect_ble_thread():
    # ... (mantida)
    connect_button.configure(state="disabled")
    if client_ble:
        asyncio.run_coroutine_threadsafe(disconnect_ble(), loop)
    else:
        status_label.configure(text="Desconectado")
        connect_button.configure(text="Conectar", command=connect_ble_thread, state="normal")
        enable_controls(False)
        send_button.configure(state="disabled")


def schedule_send_data():
    global send_task
    if send_task:
        app.after_cancel(send_task)
    
    if send_mode_automatic.get() and is_connected: # Só agenda se automático E conectado
        send_task = app.after(500, send_all_values_thread)

# send_all_values_thread é chamado diretamente pelo botão ou pelo schedule
def send_all_values_thread():
    # ... (lógica de obter valores e enviar - mantida igual)
    global is_programmatic_update
    if not is_connected:
        return

    current_values = {}
    valid_input = True
    entries_sliders = {
        "Kp": (kp_entry, kp_slider),
        "Ki": (ki_entry, ki_slider),
        "Kd": (kd_entry, kd_slider),
        "Bs": (bs_entry, bs_slider)
    }

    for name, (entry, slider) in entries_sliders.items():
        try:
            val_str = entry.get()
            if name == "Bs":
                val = int(val_str)
                min_val, max_val = slider.cget("from_"), slider.cget("to")
                val = max(int(min_val), min(val, int(max_val)))
                current_values[name] = val
            else:
                val = float(val_str)
                min_val, max_val = slider.cget("from_"), slider.cget("to")
                val = max(min_val, min(val, max_val))
                current_values[name] = val
        except ValueError:
            log_message(f"Erro: Valor inválido no campo '{name}': '{val_str}'. Não enviando.")
            status_label.configure(text=f"Erro: Valor inválido em {name}")
            valid_input = False
            is_programmatic_update = True
            current_slider_val = slider.get()
            precision = get_slider_precision(slider)
            entry.delete(0, "end")
            entry.insert(0, f"{float(current_slider_val):.{precision}f}")
            is_programmatic_update = False
            break 

    if valid_input:
        kp = current_values["Kp"]
        ki = current_values["Ki"]
        kd = current_values["Kd"]
        bs = current_values["Bs"]
        command = f"Kp={kp:.4f},Ki={ki:.4f},Kd={kd:.4f},Bs={int(bs)}"
        log_message(f"Preparando para enviar: {command}")
        asyncio.run_coroutine_threadsafe(send_ble_data(command), loop)
        status_label.configure(text="Valores enviados.")
    else:
        pass


def get_slider_precision(slider_widget):
    # ... (mantida)
    from_val = slider_widget.cget("from_")
    to_val = slider_widget.cget("to")
    steps = slider_widget.cget("number_of_steps")
    if steps == 0 or steps is None : return 0
    step_size = (to_val - from_val) / steps
    s = str(step_size)
    if '.' in s:
        precision = len(s.split('.')[1])
        return min(precision, 4)
    return 0


def on_slider_change(value, entry_widget, slider_widget):
    # ... (lógica de atualizar entry - mantida igual)
    global is_programmatic_update
    if is_programmatic_update:
        return

    is_programmatic_update = True
    precision = get_slider_precision(slider_widget)
    if slider_widget == bs_slider:
        precision = 0
        formatted_value = f"{int(float(value))}"
    else:
        formatted_value = f"{float(value):.{precision}f}"
    
    entry_widget.delete(0, "end")
    entry_widget.insert(0, formatted_value)
    is_programmatic_update = False
    schedule_send_data() # Esta função agora verifica o modo automático


def on_entry_change(event_unused, slider_widget, entry_widget, min_val, max_val, is_integer_val=False):
    # ... (lógica de atualizar slider - mantida igual)
    global is_programmatic_update
    if is_programmatic_update:
        return

    value_str = entry_widget.get()
    current_slider_val = slider_widget.get()

    try:
        if not value_str.strip():
            precision = get_slider_precision(slider_widget) if not is_integer_val else 0
            formatted_fallback = f"{float(current_slider_val):.{precision}f}" if not is_integer_val else str(int(current_slider_val))
            is_programmatic_update = True
            entry_widget.delete(0, "end")
            entry_widget.insert(0, formatted_fallback)
            is_programmatic_update = False
            return

        if is_integer_val:
            value = int(float(value_str))
        else:
            value = float(value_str)
        
        value = max(min_val, min(value, max_val))
        
        is_programmatic_update = True
        slider_widget.set(value)
        
        precision = get_slider_precision(slider_widget) if not is_integer_val else 0
        formatted_value = f"{value:.{precision}f}" if not is_integer_val else str(int(value))
        entry_widget.delete(0, "end")
        entry_widget.insert(0, formatted_value)
        is_programmatic_update = False
        
        schedule_send_data() # Esta função agora verifica o modo automático

    except ValueError:
        log_message(f"Valor inválido: '{value_str}'. Revertendo.")
        is_programmatic_update = True
        precision = get_slider_precision(slider_widget) if not is_integer_val else 0
        formatted_fallback = f"{float(current_slider_val):.{precision}f}" if not is_integer_val else str(int(current_slider_val))
        entry_widget.delete(0, "end")
        entry_widget.insert(0, formatted_fallback)
        is_programmatic_update = False

# NOVO: Função para alternar o modo de envio
def toggle_send_mode():
    update_send_button_state()
    if send_mode_automatic.get():
        log_message("Modo de envio: Automático")
        # Se mudou para automático e há valores pendentes (não implementado, mas poderia ser)
        # schedule_send_data() # Poderia enviar imediatamente ao mudar para automático
    else:
        log_message("Modo de envio: Manual")

# NOVO: Função para atualizar o estado do botão de envio manual
def update_send_button_state():
    if send_mode_automatic.get() or not is_connected:
        send_button.configure(state="disabled", fg_color=("gray70", "gray30")) # Cor opaca
    else: # Modo manual e conectado
        send_button.configure(state="normal", fg_color=ctk.ThemeManager.theme["CTkButton"]["fg_color"]) # Cor normal


def enable_controls(enable):
    # ... (mantida, mas também controla o switch de modo)
    state = "normal" if enable else "disabled"
    widgets_to_toggle = [
        kp_slider, ki_slider, kd_slider, bs_slider,
        kp_entry, ki_entry, kd_entry, bs_entry,
        send_mode_switch # Adicionado o switch aqui
    ]
    for widget in widgets_to_toggle:
        if widget: 
            widget.configure(state=state)
    # O botão de envio manual é tratado por update_send_button_state
    if enable:
        update_send_button_state()
    else:
        send_button.configure(state="disabled")


def log_message(message):
    # ... (mantida)
    if log_text.winfo_exists():
        log_text.configure(state="normal")
        log_text.insert("end", message + "\n")
        log_text.configure(state="disabled")
        log_text.see("end")

def on_closing():
    # ... (mantida)
    log_message("Fechando aplicação...")
    if client_ble and is_connected and loop and loop.is_running():
        future = asyncio.run_coroutine_threadsafe(disconnect_ble(), loop)
        try:
            future.result(timeout=3) 
            log_message("Desconexão BLE finalizada.")
        except asyncio.TimeoutError:
            log_message("Timeout ao tentar desconectar o BLE.")
        except Exception as e:
            log_message(f"Erro ao desconectar na saída: {e}")
    
    if loop and loop.is_running():
        loop.call_soon_threadsafe(loop.stop)
    app.destroy()

# --- Configuração da Interface Gráfica ---
ctk.set_appearance_mode("System")
ctk.set_default_color_theme("blue")

app = ctk.CTk()
app.title("Controle ESP32 BLE")
app.geometry("550x650") # Aumentado um pouco para o switch

send_mode_automatic = ctk.BooleanVar(value=True) # Agora está OK

main_frame = ctk.CTkFrame(app)
main_frame.pack(pady=10, padx=10, fill="both", expand=True)

# Frame para Conexão e Switch de Modo de Envio
top_controls_frame = ctk.CTkFrame(main_frame)
top_controls_frame.pack(pady=5, fill="x")

connect_button = ctk.CTkButton(top_controls_frame, text="Conectar", command=connect_ble_thread, width=100)
connect_button.pack(side="left", padx=5)

status_label = ctk.CTkLabel(top_controls_frame, text="Status: Desconectado", anchor="w")
status_label.pack(side="left", padx=(5,10), fill="x", expand=True)

# NOVO: Switch para modo de envio
send_mode_switch = ctk.CTkSwitch(top_controls_frame, text="Envio Automático", variable=send_mode_automatic, command=toggle_send_mode)
send_mode_switch.pack(side="right", padx=5)


controls_frame = ctk.CTkFrame(main_frame)
controls_frame.pack(pady=5, fill="both", expand=True)

def create_control_row(parent, label_text, slider_from, slider_to, slider_steps, initial_val_str, is_int=False):
    # ... (mantida)
    frame = ctk.CTkFrame(parent)
    frame.pack(fill="x", pady=3)
    ctk.CTkLabel(frame, text=label_text, width=90).pack(side="left", padx=(5,0))

    entry = ctk.CTkEntry(frame, width=80)
    entry.pack(side="left", padx=5)

    slider = ctk.CTkSlider(frame, from_=slider_from, to=slider_to, number_of_steps=slider_steps)
    
    current_val = float(initial_val_str)
    slider.set(current_val)
    precision = 0 if is_int else get_slider_precision(slider)
    formatted_initial = str(int(current_val)) if is_int else f"{current_val:.{precision}f}"
    entry.insert(0, formatted_initial)

    entry.bind("<Return>", lambda e, s=slider, en=entry, f=slider_from, t=slider_to, i=is_int: on_entry_change(e, s, en, f, t, i))
    entry.bind("<FocusOut>", lambda e, s=slider, en=entry, f=slider_from, t=slider_to, i=is_int: on_entry_change(e, s, en, f, t, i))
    slider.configure(command=lambda val, en=entry, sl=slider: on_slider_change(val, en, sl))
    
    slider.pack(side="left", fill="x", expand=True, padx=5)
    return slider, entry

# Criando os controles (sliders e entries)
kp_slider, kp_entry = create_control_row(controls_frame, "Kp:", 0, 10, 200, "2.4")
ki_slider, ki_entry = create_control_row(controls_frame, "Ki:", 0, 0.1, 1000, "0.0020")
kd_slider, kd_entry = create_control_row(controls_frame, "Kd:", 0, 50, 500, "7.0")
bs_slider, bs_entry = create_control_row(controls_frame, "Base Speed:", 0, 255, 255, "80", is_int=True)

# Botão de Envio Manual
send_button = ctk.CTkButton(main_frame, text="Enviar Valores Agora", command=send_all_values_thread)
send_button.pack(pady=10)

log_label = ctk.CTkLabel(main_frame, text="Log:")
log_label.pack(pady=(5,0), anchor="w")
log_text = ctk.CTkTextbox(main_frame, height=100)
log_text.pack(pady=5, fill="both", expand=True)
log_text.configure(state="disabled")

# Inicializar widgets com estado desabilitado
enable_controls(False) # Desabilita sliders, entries e switch
send_button.configure(state="disabled") # Garante que o botão de envio comece desabilitado
# O switch começa em automático, então o botão de envio deve começar opaco/desabilitado
update_send_button_state() # Chama para definir o estado inicial do botão de envio


# --- Loop Principal e Thread Asyncio (MANTIDO IGUAL) ---
# ... (run_asyncio_loop e if __name__ == "__main__": ...)
def run_asyncio_loop(loop_to_run):
    try:
        loop_to_run.run_forever()
    except KeyboardInterrupt:
        if log_text.winfo_exists(): log_message("Loop asyncio interrompido.")
    finally:
        if loop_to_run.is_running():
            tasks = [task for task in asyncio.all_tasks(loop=loop_to_run) if not task.done()]
            for task in tasks:
                task.cancel()
            if log_text.winfo_exists(): log_message(f"Cancelando {len(tasks)} tarefas asyncio.")
        if not loop_to_run.is_closed():
             loop_to_run.close()
        if log_text.winfo_exists(): log_message("Loop asyncio fechado.")


if __name__ == "__main__":
    try:
        loop = asyncio.get_running_loop()
    except RuntimeError:
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)

    from threading import Thread
    asyncio_thread = Thread(target=run_asyncio_loop, args=(loop,), daemon=True)
    asyncio_thread.start()

    app.protocol("WM_DELETE_WINDOW", on_closing)
    app.mainloop()

    if log_text.winfo_exists(): log_message("GUI fechada.")
    if asyncio_thread.is_alive():
        if log_text.winfo_exists(): log_message("Aguardando thread asyncio finalizar...")
        if loop and loop.is_running():
            loop.call_soon_threadsafe(loop.stop)
        asyncio_thread.join(timeout=3)
        if asyncio_thread.is_alive():
            if log_text.winfo_exists(): log_message("Thread asyncio não finalizou a tempo.")
        else:
            if log_text.winfo_exists(): log_message("Thread asyncio finalizada.")
    else:
        if log_text.winfo_exists(): log_message("Thread asyncio já estava finalizada.")