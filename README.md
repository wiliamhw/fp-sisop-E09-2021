# Info
* Server hanya dapat menerima satu user yang sedang login.
* Semua tabel memiliki ekstensi `.csv`.

# Setup
1. Buat program server-client multi koneksi berdasarkan pengerjaan dari modul 2 no.1.
   * Program server bernama `server.c`.
   * Program client bernama `client.c`.
2. Buka `~/.bashrc`, lalu tambahkan `export PATH="$PATH:</path/to/client>"` di baris terakhir.
   * Langkah ini dilakukan agar program client bisa diakses dari direktori apapun.
3. Jalankan server pada Daemon.
4. Untuk mematikan server Daemon, lakukan langkah berikut:
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
1. Buat fungsi yang mendeteksi apakah yang akan login adalah root atau bukan.
   * Jika login degan root, maka `geteuid() == 0`.
2. Jika pengguna tidak login sebagai root, dapatkan username dan password dari command line argument.
   * Format perintah: `client -u <username> -p <password>`.
3. Cek username dan passwod di tabel **users** (di dalam db **config**).
   1. Jika ada, dapatkan id dari user tersebut.
   2. Jika tidak ada, tampilkan **Error, invalid username/password**.
4. Simpan id dari current user di server.
   * Jika root, `id = 0`.
5. Tampilkan tulisan **Login berhasil** ke client.
6. Untuk setiap new line pada terminal client, tuliskan `<tipe akun>@id:` di bagian kiri terminal.

### Fitur Register
1. Pastikan bahwa user yang sedang login saat ini adalah root.
   * Jika bukan, tampilkan **Error, user tidak diizinkan untuk membuat user baru**.
2. Dapatkan username dan password akun user baru dari command line argument.
   * Format perintah: `CREATE USER <username> IDENTIFIED BY <password>`.
3. Pastikan tidak ada username yang duplikat.
   * Jika ada, tampilkan **User telah terdaftar**.
4. Masukan username, password, beserta id ke tabel **users**.
   * Format: `id,username,password`
5. Tampilkan tulisan **Register berhasil** ke client.
