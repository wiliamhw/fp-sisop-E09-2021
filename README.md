# Info
* Server hanya dapat menerima satu user yang sedang login.
* Semua tabel memiliki ekstensi `.csv`.
* Query tidak boleh melebihi 20 kata.

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
   * Jika argument tidak valid, tampilkan **Error::Invalid argument**.
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
   * Jika bukan, tampilkan pesan **Error::Forbidden action**.
2. Dapatkan username dan password akun user baru dari command line argument.
   * Format perintah: `CREATE USER <username> IDENTIFIED BY <password>;`.
3. Validasi input di bagian client. **TODO**
   * Cek apakah format query sudah sesuai.
4. Pastikan tidak ada username yang duplikat di db.
   * Jika ada, tampilkan **Error::User is already registered**.
5. Masukan username, password, beserta id ke tabel **users**.
   * Format: `id,username,password`
6. Tampilkan tulisan **Register success** ke client.


# Authorisasi
## Penjelasan
* User hanya bisa mengakses database yang diizinkan.
* Root bisa akses semua database, termasuk db **config**.
* Root bisa memberikan izin akses untuk user dengan format `GRANT PERMISSION <nama_database> INTO <nama_user>;`
* Untuk menggunakan database, tuliskan perintah `USE <nama_database>`.

## Penyelesaian
* Buat tabel **permissions** di db **config** dengan kolom `id,nama_table` yang menyimpan tabel beserta id milik user yang dapat mengaksesnya.
* Tambahkan perintah *grant permission* dan *use* pada fungsi `routes`.

### Fitur Akses Database
1. Dapatkan id dari user saat ini dan nama database yang ingin diakses.
2. Pastikan folder database ada pada server.
   * Jika tidak ada, tampilkan tulisan **Error::Database not found** pada client.
   * Jika ada, lakukan langkah selanjutnya.
3. Jika pengguna tidak menggunakan akun root, cek tabel **permissions**.
   1. Jika id dan nama database ada pada tabel tersebut, lakukan langkah berikutnya.
   2. Jika tidak, tampilkan tulisan **Error::Unauthorized access** pada client. 
4. Simpan nama database ke dalam variabel `curr_db` pada server.
5. Kirim nama database ke client.
6. Ganti info pada terminal menjadi `<username>@<nama database>`.

### Fitur Pemeberian Akses Database
1. Pastikan pengguna yang menggunakan perintah ini adalah root.
   * Jika tidak, tampilkan tulisan **Error::Forbidden action**.
2. Dapatkan nama database dan nama user.
2. Pastikan folder database ada pada server.
   * Jika tidak ada, tampilkan tulisan **Error::Database not found** pada client.
   * Jika ada, lakukan langkah selanjutnya.
4. Dapatkan id target dari nama user pada tabel **users**.
   * Jika user tidak ada pada db, tampilkan tulisan **Error::User not found** pada client.
5. Cek tabel **permissions**.
   1. Jika id target dan nama database ada pada tabel tersebut, tampilkan tulisan **Info::User already authorized** pada client.
   2. Jika tidak, lakukan langkah berikutnya.
6. Masukan id dan nama database ke tabel **permissions** dengan format `<user id>,<nama database>`.
7. Pada client, tampilkan tulisan **Permission added**.
