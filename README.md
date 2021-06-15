# Info
* Server hanya dapat menerima satu user yang sedang login.
* Semua tabel memiliki ekstensi `.csv`.

# Setup
1. Buat program server-client multi koneksi berdasarkan pengerjaan dari modul 2 no.1.
   * Program server bernama `server.c`.
   * Program client bernama `client.c`.
2. Buka `~/.bashrc`, lalu tambahkan `export PATH="$PATH:</path/to/client/dir>"` di baris terakhir.
   * Langkah ini dilakukan agar program client bisa diakses oleh user dari direktori apapun.
3. Buka `/etc/sudoers`, lalu `export PATH="$PATH:</path/to/client/dir>"` di baris terakhir.
   * Langkah ini dilakukan agar program client bisa diakses oleh root dari direktori apapun.
4. Jalankan server pada Daemon.
5. Untuk mematikan server Daemon, lakukan langkah berikut:
   1. Cari pid dari server dengan perintah `ps -aux | grep "server"`.
   2. Kill server dengan perintah `sudo kill -9 <pid>`.


# Autentikasi
## Penjelasan
* Root bisa login dan menambahkan user.
* User biasa hanya bisa login.
* Format untuk menambahkan user: `CREATE USER <username> IDENTIFIED BY <password>`.
* Format untuk login sebagai user: `client -u <username> -p <password>`.
* Format untuk login sebagai root: `sudo client`

## Penyelesaian
### Fitur Login
1. Patikan tidak ada akun yang sedang login.
   * Jika ada akun lain, tulis **Server is busy, wait for other user to logout.** di terminal client.
2. Jika pengguna tidak login sebagai root, dapatkan username dan password dari command line argument.
   * Format perintah: `client -u <username> -p <password>`.
3. Validasi argument di sisi client dan kirim ke server dengan format `login <username> <password>`.
   * Jika yang login adalah root, kirim `login root` ke server.
   * Jika argument tidak valid, tampilkan **Argumen tidak valid**.
4. Cek username dan password di tabel **users** (di dalam db **config**).
   1. Jika ada, dapatkan id dari user tersebut.
   2. Jika tidak ada, tampilkan **Error::Invalid id or password**.
5. Simpan id dari current user di server.
   * Jika root, `id = 0`.
6. Jika login gagal, tampilkan pesan gagal dari server ke client.
7. Jika sukses, tampilkan tulisan **Login success** ke client.
8. Untuk setiap new line pada terminal client, tuliskan `<tipe akun>@<username>:` di bagian kiri terminal.
   * Ada dua tipe akun, yaitu **user** dan **root**.
9.  Untuk keluar, tuliskan perintah `quit` atau tekan `Ctrl + C` pada client.

### Fitur Register
1. Pastikan bahwa user yang sedang login saat ini adalah root.
   * Jika bukan, tampilkan pesan error.
2. Dapatkan username dan password akun user baru dari command line argument.
   * Format perintah: `CREATE USER <username> IDENTIFIED BY <password>;`.
3. Validasi input di bagian client. **TODO**
   * Cek apakah format query sudah sesuai.
4. Pastikan tidak ada username yang duplikat di db.
   * Jika ada, tampilkan **Error::User is already registered**.
5. Masukan username, password, beserta id ke tabel **users**.
   * Format: `id,username,password`
6. Tampilkan tulisan **Register success** ke client.
