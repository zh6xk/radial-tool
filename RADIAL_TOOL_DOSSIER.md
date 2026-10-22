# PROJECT BRIEF & DEBUGGING DOSSIER: RADIAL TOOL (v2.0 DIRECT2D UPGRADE)

Dokumen ini adalah ringkasan teknis, arsitektur, dan rekam jejak pembangunan **Radial Tool**.  
Gunakan dokumen ini sebagai **handover context** ketika menghadapi bug persisten, refactoring besar, atau saat meminta audit eksternal/AI lain agar tidak mengulang investigasi dari nol.

---

## 1. TUJUAN UTAMA PROYEK

Membangun aplikasi desktop Windows **native C++ (Win32 API)** berupa **Radial / Pie Action Wheel & Universal Quick Launcher** yang diaktifkan instan via mouse side buttons (**M4 / M5 / XBUTTON1 / XBUTTON2**).

### Kriteria Non-Negosiabel:
1. **Ultra Lightweight**:
   - Memory footprint (RAM): 5 – 15 MB (tidak boleh memakai Electron, WebView2, atau runtime berat).
   - CPU saat idle: **0.0%**.
   - Binary standalone tunggal (< 2 MB), portable, siap jalan tanpa installer rumit.
2. **Instant Response & Zero Lag**:
   - Respon pembukaan wheel instan di bawah kursor mouse tanpa blocking OS mouse thread.
3. **Smart Explorer Context**:
   - Deteksi otomatis file yang sedang diseleksi di Windows Explorer (termasuk Explorer modern bertab Windows 11) dan Desktop.
   - Hasil eksekusi (convert, compress, PDF) wajib jatuh tepat di folder yang sama di samping file sumber.
4. **Customizable & Modern Ergonomics**:
   - Maksimal 5 sektor per halaman wheel (sudut lebar ~72° agar akurat).
   - Navigasi antar halaman wheel menggunakan **Scroll Mouse Wheel**.
   - Navigasi gestural arah mouse (*flick navigation* via polar angle tanpa repot menembak kursor ke tombol).
   - Tombol tengah sebagai tombol **Close / Cancel (`✕`)**.
   - Web launcher dengan rendering logo / favicon otomatis dari link yang didaftarkan.

---

## 2. ARSITEKTUR TEKNIS & STACK

| Komponen | Implementasi Lama (v1.0 - v1.4) | Implementasi Baru (v2.0 Target) |
|---|---|---|
| **Bahasa & Toolchain** | C++17 / MinGW-w64 (`x86_64-w64-mingw32-g++`) | C++17 / MinGW-w64 (`x86_64-w64-mingw32-g++`) |
| **Window Subsystem** | Win32 API (`WS_EX_LAYERED`, `WS_EX_TOPMOST`, `WS_POPUP`) | Win32 API (`WS_EX_LAYERED`, `WS_EX_TOPMOST`, `WS_POPUP`) |
| **Rendering Engine** | GDI+ (`GdiplusStartup`, `Graphics`, `Image`) | **Direct2D 1.1 + DirectWrite** (GPU-accelerated, subpixel anti-aliasing) |
| **Input Hooking** | Low-Level Mouse Hook (`WH_MOUSE_LL`) | `WH_MOUSE_LL` + Asynchronous `PostMessage` |
| **Smart Detect** | COM: `IShellWindows` + `IFolderView2::Items` / `CF_HDROP` | COM: `IShellWindows` + `IFolderView2::Items` / `CF_HDROP` |
| **PDF Engine** | Native manual PDF 1.4 binary stream generator | Native manual PDF 1.4 binary stream + PDF Unmerge/Split parser |
| **Konfigurasi** | Hardcoded C++ arrays | `config.json` lokal (warna, halaman, URL, shortcut) |

---

## 3. RIWAYAT FITUR & HISTORI PEKERJAAN (YANG SUDAH BERHASIL)

1. **PDF Split & Unmerge Engine (v1.9)**:
   - Fitur "PDF Split" di layer Media aktif secara penuh sebagai alat Unmerge sejati.
   - Smart PDF Target Detection via Explorer (`g_smartFiles`) dengan fallback dialog `PickPdfFile`.
   - Native Stream Extraction: Memindai struktur stream PDF secara binary mencari stream gambar (JPEG `0xFF 0xD8 0xFF ... 0xFF 0xD9`).
   - GDI+ Stream Validation: Memverifikasi keabsahan tiap gambar melalui `CreateStreamOnHGlobal` + `Gdiplus::Bitmap` sebelum diekstrak.
   - Clean Unmerge Output:
     - Hanya mengekstrak dan menyimpan file gambar asli (`.jpg`) tanpa menduplikasi file `.pdf` tambahan agar folder tetap rapi.
     - Otomatis membersihkan suffix `-merged` pada nama file asal (contoh: `foto-merged.pdf` kembali menjadi `foto-page-01.jpg`, `foto-page-02.jpg`).
   - Notifikasi Balon Tray `ShowTrayBalloon` dan refresh instan di folder Explorer via `SHChangeNotify(SHCNE_CREATE)` & `SHCNE_UPDATEDIR`.
2. **Dashboard Pengaturan Desktop Native (Settings GUI Window) (v1.8)**:
   - Akses via klik kanan Tray Icon -> **"Pengaturan Wheel (Settings)"**.
   - GUI Desktop Native Win32 (`TabControl`, `ListBox`, `EditBox`, `Button`) tanpa browser/Electron.
   - Fitur Pengaturan:
     - Tab **Media Tools (Page 0)** dan Tab **Web Launcher (Page 1)**.
     - Tambah sektor baru (`+ Tambah / Update`) dengan nama dan URL/target custom.
     - Hapus sektor (`- Hapus Sektor`).
     - Reset ke susunan bawaan (`Reset Default`).
     - Simpan dan terapkan langsung (`Simpan & Terapkan`).
     - Validasi batas ergonomis: Maksimal 5 sektor per layer wheel agar arah navigasi flick tetap presisi.
   - Konfigurasi tersimpan otomatis di `%APPDATA%\RadialTool\config.ini`.
   - Zero background overhead: Ketika jendela pengaturan ditutup, seluruh resource GUI di-destroy dan RAM langsung kembali ke **~2–5 MB** via `SetProcessWorkingSetSize`.
2. **Ultra-Lightweight Memory Optimization (v1.7)**:
   - Menambahkan fungsi `DismissWheel(HWND hWnd)`:
     - Mengosongkan cache vektor file seleksi Explorer (`g_smartFiles.clear()` & `shrink_to_fit()`).
     - Mengembalikan alokasi memori proses ke level minimum dengan Windows API `SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1)`.
     - Hasil: RAM kembali turun ke ~2–5 MB saat idle, 0% CPU consumption.
2. **Mekanisme Navigasi "Hold-Aim-Release" (v1.6)**:
   - Terinspirasi dari arsitektur Rovyl (`HenryCauan/rovyl`):
     - **Hold**: Tahan tombol trigger M4/M5 (`WM_XBUTTONDOWN`) untuk memunculkan wheel tepat di bawah kursor.
     - **Aim**: Arahkan mouse (flick gestural) ke sektor yang dituju.
     - **Release**: Lepas tombol trigger (`WM_XBUTTONUP`) untuk langsung mengeksekusi aksi sektor tersebut tanpa perlu klik kiri terpisah.
     - **Cancel Protection**: Jika tombol dilepas saat kursor masih berada di lingkaran tengah (`g_hovered == -2`), wheel otomatis menutup tanpa mengeksekusi apapun.
   - Dual-input support: Mode klik kiri biasa tetap aktif bagi yang terbiasa klik manual.
   - Penyesuaian visual modern:
     - Ambient drop shadow halus di lingkar luar wheel.
     - Pemisahan tiap sektor dengan radial separator gap (`gapAngle = 2.2°`) ala Rovyl (tidak lagi menyatu kaku).
     - Tipografi Segoe UI ClearType anti-aliased dengan penambahan icon/glyph khusus di atas teks label tiap sektor.
     - Warna sektor: Obsidian Dark Glass 90% opacity dengan Vibrant Electric Cyan glow saat di-hover.
2. **Multi-Page Wheel (Scroll Paging) & Web Launcher (v1.5)**:
   - Wheel dibatasi tepat **maksimal 5 sektor per halaman** agar sudut ergonomis (~72 derajat) akurat dan mudah dijangkau gestur flick.
   - Paging 2 layer dikendalikan dengan **Scroll Mouse Wheel** (`WM_MOUSEWHEEL` via hook forwarding `WM_APP_SCROLLWHEEL`):
     - **Page 0 (MEDIA)**: Format Image, Image to PDF, Screenshot, Compress, PDF Split.
     - **Page 1 (WEB)**: YouTube, Netflix, GitHub, ChatGPT, Google (membuka langsung via `ShellExecuteW`).
   - Indikator halaman visual (`● ○` / `○ ●`) dan nama layer (`MEDIA` / `WEB`) ditampilkan di lingkaran tengah.
2. **Center Cancel Button & Gestural Flick Selection**:
   - Teks "TORA" di lingkaran tengah resmi diganti menjadi tombol **Close / Cancel (`✕`)**.
   - Hover di lingkaran tengah berubah warna merah lembut (`✕ BATAL` / `✕ Tutup`), dan jika diklik langsung menutup wheel (`ShowWindow(hWnd, SW_HIDE)`).
   - Menghapus batas `dist > R_OUTER`: seleksi sektor kini berbasis **polar angle vector** ($\text{atan2}(dy, dx)$) murni mengikuti arah dorongan/flick mouse keluar dari tengah tanpa harus kursor tepat berhenti di atas ring.
2. **Low-Latency Asynchronous Hook**:
   - `LowLevelMouseProc` hanya menangkap `WM_XBUTTONDOWN`, mengambil posisi kursor dan foreground window, lalu mengirim pesan custom `WM_APP_OPENWHEEL (WM_APP + 1)` ke window procedure utama, kemudian langsung `return 1`.
   - Menghilangkan freeze/lag pada kursor mouse di Windows.
2. **Windows 11 Tabbed Explorer Detection**:
   - Melakukan loop pada `IShellWindows`.
   - Mengambil `IOleWindow::GetWindow` dan memvalidasi class name `ShellTabWindowClass` yang visible (`IsWindowVisible`) untuk membedakan tab yang aktif di foreground dari tab latar belakang.
3. **Dual-Path Extraction**:
   - Menggunakan `IFolderView2::Items(SVGIO_SELECTION, IID_IShellItemArray)` sebagai jalur utama Windows modern.
   - Fallback otomatis ke `IDataObject (CF_HDROP)` jika jalur modern kosong (misal di Desktop legacy).
4. **Image Converter & Smart Compressor**:
   - Konversi format PNG, JPG, WebP.
   - Kompresi JPEG dengan parameter kualitas 60.
   - **White canvas compositing**: Menangani transparansi PNG yang diubah ke JPEG agar latar belakang tidak menjadi hitam pekat (`Graphics::Clear(Color::White)`).
5. **Native PDF 1.4 Merge**:
   - Menyatukan banyak gambar menjadi 1 file PDF biner manual tanpa library eksternal.
   - Menangani orientasi EXIF (`PropertyTagOrientation` 0x0112) dengan rotasi otomatis sebelum dimasukkan ke PDF.
   - Menggunakan IStream via `CreateStreamOnHGlobal` dan `GetHGlobalFromStream`.
   - Output dinamai otomatis `<nama-pertama>-merged.pdf` via `MakeUniquePath()`.
6. **Shell Integration & Feedback**:
   - Memanggil `SHChangeNotify(SHCNE_CREATE)` dan `SHChangeNotify(SHCNE_UPDATEDIR)` agar file output langsung terlihat di folder Explorer tanpa perlu refresh manual (F5).
   - Notifikasi balon tray native via `Shell_NotifyIconW(NIM_MODIFY)` dengan flag `NIF_INFO`.

---

## 4. JEBAKAN & AKAR MASALAH YANG PERNAH DIHADAPI (LESSONS LEARNED)

Jika suatu saat fitur gagal berfungsi atau hasil tidak muncul, periksa poin-poin krusial ini:

### A. Pitfall Format String MinGW (`swprintf`)
- **Masalah**: Pada compiler MinGW-w64, format specifier `%s` di dalam varian `swprintf` diperlakukan sebagai narrow string (`const char*`), BUKAN wide string (`const wchar_t*`).
- **Dampak Fatal**: Path string `WCHAR*` terpotong hanya pada karakter pertama (misal `C:\Foto` terbaca sebagai `C`), menyebabkan `MakeUniquePath` menghasilkan path korup seperti `C\7-.j` dan fungsi penyimpanan file gagal total secara diam-diam.
- **Aturan Baku**:
  - Gunakan selalu **`%ls`** untuk semua string wide `wchar_t*` atau `WCHAR*`.
  - Gunakan **`%hs`** untuk string narrow (seperti macro `__DATE__` dan `__TIME__`).
  - Untuk manipulasi path, utamakan menggunakan **`std::wstring`** native daripada variadic `swprintf`.

### B. Shell Windows Tab Hunting
- Tab Explorer Windows 11 berbagi satu PID dan process host yang sama. Jika loop `IShellWindows` tidak menyaring window visibility (`IsWindowVisible`) dan class `ShellTabWindowClass`, seleksi tab yang tidak aktif bisa menimpa seleksi tab yang sedang aktif di depan user.

### C. GDI+ Encoder CLSID
- Windows GDI+ tidak mendukung encoder WebP secara native out-of-the-box di beberapa build Windows lawas (hanya JPEG, PNG, BMP, TIFF, GIF). Saat migrasi ke Direct2D/WIC (Windows Imaging Component), WIC memiliki dukungan decoder/encoder yang lebih luas dan terisolasi.

---

## 5. RENCANA PENGEMBANGAN v2.0 (NEXT STEP)

1. **Rendering Modern**:
   - Mengganti engine `gdiplus` dengan `d2d1` (Direct2D 1.1) + `dwrite` (DirectWrite) + `windowscodecs` (WIC).
2. **Navigasi Ergonomis**:
   - Tombol tengah lingkaran diubah menjadi tombol **Close (`✕`)**.
   - Input arah mouse (*polar angle calculation* via delta $\Delta x, \Delta y$) untuk memilih sektor secara gestural tanpa kursor mouse fisik.
   - Navigasi antar set wheel (maksimal 5 item per halaman) menggunakan scroll mouse.
3. **Fitur Lanjutan yang Disetujui**:
   - **PDF Unmerge / Split**: Memecah halaman PDF kembali menjadi file terpisah atau gambar.
   - **Quick OCR**: Salin teks dari gambar/screenshot langsung ke clipboard via native `Windows.Media.Ocr`.
   - **Quick Video to GIF**: Konverter klip video instan.
   - **Lossless EXIF Stripper**: Pembersih metadata privasi/lokasi gambar.
   - **Desktop Color Picker**: Eyedropper layar pencatat kode warna HEX/RGB.
   - **Web Quick Launcher**: Sektor web yang menampilkan logo/favicon otomatis dari tautan yang didaftarkan.

---

## 6. LOKASI FILE & ENDPOINT DEPLOYMENT

- **Source Code C++**: `/home/azureuser/radial-wheel-app/main.cpp`
- **Output Executable**: `/home/azureuser/radial-wheel-app/RadialTool.exe`
- **Public Binary Download**: `https://jsibra.me/downloads/RadialTool.exe`
- **Public Source Code**: `https://jsibra.me/downloads/RadialTool_main.cpp`
- **Public Project Dossier**: `https://jsibra.me/downloads/RADIAL_TOOL_DOSSIER.md`
- **Runtime Log Client**: `%TEMP%\RadialTool.log`
