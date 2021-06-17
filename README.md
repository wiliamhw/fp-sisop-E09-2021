# Info
* Server hanya dapat menerima satu user yang sedang login.
* Semua tabel memiliki ekstensi `.csv`.
<br><br>

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
<br><br>

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
3. Validasi input di bagian client. *TODO*
   * Cek apakah format query sudah sesuai.
4. Pastikan tidak ada username yang duplikat di db.
   * Jika ada, tampilkan **Error::User is already registered**.
5. Masukan username, password, beserta id ke tabel **users**.
   * Format: `id,username,password`
6. Tampilkan tulisan **Register success** ke client.
<br><br>

# Authorisasi
## Penjelasan
* User hanya bisa mengakses database yang diizinkan.
* Root bisa akses semua database, termasuk db **config**.
* Root bisa memberikan izin akses untuk user dengan format `GRANT PERMISSION <nama_database> INTO <nama_user>;`
* Untuk menggunakan database, tuliskan perintah `USE <nama_database>;`.

## Penyelesaian
* Buat tabel **permissions** di db **config** dengan kolom `id,nama_table` yang menyimpan tabel beserta id milik user yang dapat mengaksesnya.
* Tambahkan perintah *grant permission* dan *use* pada fungsi `routes`.

### Fitur Akses Database
1. Dapatkan id dari user saat ini dan nama database yang ingin diakses dari client dan kirim ke server.
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
2. Dapatkan nama database dan nama user dari client dan kirim ke server.
3. Pastikan folder database ada pada server.
   * Jika tidak ada, tampilkan tulisan **Error::Database not found** pada client.
   * Jika ada, lakukan langkah selanjutnya.
4. Dapatkan id target dari nama user pada tabel **users**.
   * Jika user tidak ada pada db, tampilkan tulisan **Error::User not found** pada client.
5. Cek tabel **permissions**.
   1. Jika id target dan nama database ada pada tabel tersebut, tampilkan tulisan **Info::User already authorized** pada client.
   2. Jika tidak, lakukan langkah berikutnya.
6. Masukan id dan nama database ke tabel **permissions** dengan format `<user id>,<nama database>`.
7. Pada client, tampilkan tulisan **Permission added**.
<br><br>

# Data Definition Language (DDL)
## Penjelasan
* Input hanya berupa angka dan huruf.
* User yang membuat database otomatis memiliki permission untuk database tersebut.
* Perintah untuk membuat database adalah: `CREATE DATABASE <nama database>;`.
* Jika root dan user dapat mengakses database, maka dia juga dapat:
   1. Membuat tabel untuk database tersebut.
      * Perintah untuk membuat tabel adalah: `CREATE TABLE <nama tabel> (<nama kolom> <tipe data>, ...);`
      * Contoh: `CREATE TABLE table1 (kolom1 string, kolom2 int, kolom3 string, kolom4 int);`
   2. Menghapus (drop) database, tabel, dan kolom.
      * Perintah untuk meng-drop adalah: `DROP <DATABASE | TABLE | COLUMN> <nama_database | nama_tabel | <nama_kolom> FROM <nama_tabel>;`
      * Contoh: 
         ```
         DROP DATABASE database1;
         DROP TABLE table1;
         DROP COLUMN kolom1 FROM table1;
         ```

## Penyelesaian
### Fitur Create Database
1. Dapatkan nama database yang akan dibuat dari client dan kirim ke server.
   * Dari perintah: `CREATE DATABASE <nama database>;`
2. Pastikan db yang akan dibuat belum ada di server.
   * Jika sudah ada, tampilkan **Error::Database already exists** pada client.
3. Buat database baru dengan nama yang diberikan oleh client.
4. Jika client saat ini menggunakan akun user, tambahkan id user dan nama database ke tabel **permissions**.
5. Tampilkan **Database created** pada client.

### Fitur Create Table
1. Pastikan client sedang menggunakan suatu database.
   * Jika tidak, tampilkan **Error::No database used** pada client.
2. Dapatkan nama tabel beserta kolom-kolomnya dari client dan kirim ke server.
   * Dari perintah: `CREATE TABLE <nama tabel> (<nama_kolom> <tipe_data>, ...);`
3. Pastikan tabel yang akan dibuat belum ada di database yang sedang digunakan pada server.
   * Jika sudah ada, tampilkan **Error::Table already exists**.
4. Buat tabel baru pada database yang saat ini sedang digunakan.
5. Tampilkan **Table created** pada client.

### Fitur Drop
#### Fungsi Delete Table
1. Buat fungsi `bool deleteTable(fd, <db_name>, <table>, <column>, <value>, printSuccess)`.
   * `fd` adalah file descriptor milik socket client.
   * `printSuccess` adalah booelean yang menandakan apakah success message pada fungsi ini akan ditampilkan kepada client atau tidak.
   * Fungsi ini akan mereturn `true` bila sukses dan `false` bila terjadi error.
2. `db_used = (<db_name> != NULL) ? db_name : curr_db`.
3. Pastikan `db_used` tidak NULL.
   * Jika tidak, tampilkan **Error::No database specified on delete** pada client.
4. Dapatkan nama tabel yang akan didelete.
5. Dapatkan jenis delete berdasarkan nilai `<column>` dan `value`.
   * Jika `<column>` dan `<value>` bernilai `NULL`, hapus semua baris pada `<table>` (**hapus tabel**).
   * Jika `<column>` tidak `NULL` dan `<value>` bernilai `NULL`, hapus `column` pada `table`. (**hapus kolom**)
   * Jika tidak ada argumen yang bernilai `NULL`, hapus baris pada `<table>` dimana `<column>` bernilai `<value>`. (**hapus baris tertentu**)
6. Jika jenisnya adalah **hapus tabel**:
   1. Hapus semua baris pada tabel dengan menggunakan `fopen(<table>, "w")`.
   2. Jika `printSuccess == true`, tampilkan **Table deleted** pada client.
7. Selain itu, lakukan langkah dibawah ini.
8.  Dapatkan baris pertama pada `table`.
9.  Pecah baris pertama tersebut per-kata dan simpan di dalam sebuah array.
10. Dapatkan index `<column>` pada array di langkah sebelum ini. 
   * Jika tidak ada, tampilkan **Error::Collumn not found** pada client dan return `false`.
11. Buat tabel baru pada DB yang sedang diakses dengan nama `new-<table>`.
12. Jika jenis delete adalah **hapus kolom**:
   1. Untuk masing-masing baris pada `<tabel>`:
      1. Pecah baris tersebut per-kata dan simpan di dalam sebuah array.
      2. Untuk masing-masing kata pada array di langkah sebelum ini:
         1. Dapatkan index kata tersebut pada array.
         2. Jika index tersebut tidak sama dengan index `<column>`, copy kata tersebut ke `new-<table>`.
         3. Jika index saat ini bukan index terakhir, print `,` pada `new-<table>`.
      3. Print `\n` pada `new-<table>` untuk menandakan pergantian baris.
   2. Jika `printSuccess == true`, tampilkan **Collumn dropped** pada client.
13. Jika jenis delete adalah **hapus baris tertentu**:
   3. Initialisasi sebuah counter yang menyimpan banyak baris yang terhapus dengan 0.
   4. Untuk masing-masing baris pada `<tabel>`:
      1. Pecah baris tersebut per-kata dan simpan di dalam sebuah array.
      2. Jika baris tersebut pada index `<column>` tidak bernilai `<value>`, copy baris tersebut ke `new-<table>`.
      3. Jika tidak, increment counter baris yang terhapus.
      4. Print `\n` pada `new-<table>` untuk menandakan pergantian baris.
   5. Jika `printSuccess == true`, tampilkan **Delete success, `<counter>` row has been deleted** pada client, dimana `<counter>` adalah banyaknya baris yang terhapus.
14. Hapus `<table>` dan ubah nama `new-<table>` menjadi `<table>`.
15. Return true;

#### Fitur Drop Database
1. Dapatkan database yang akan didrop dari client dan kirim ke server.
   * Dari perintah: `DROP DATABASE <db_name>`
2. Pastikan database yang dihapus tidak bernama `config`.
   * Jika statement di atas salah, tampilkan **Error:Can't drop configuration database**.
3. Jika database yang akan di drop sedang digunakan:
   1. Kosongkan `curr_db` pada server.
   2. Ganti `type` pada client sesuai dengan tipe akun client.
   3. Lewati langkah ke-4 dn ke-5.
4. Pastikan database ada pada server.
   * Jika tidak ada, tampilkan tulisan **Error::Database not found** pada client.
5. Pastikan client memiliki permissions pada database.
   * Jika tidak, tampilkan **Error::Unauthorized access**.
6. Hapus database pada server.
7. Pada tabel permissions, hapus semua baris dengan kolom `db_name == <nama database yang terhapus>` dengan perintah `deleteTable(fd, "config", "permissions", "db_name", <nama database yang terhapus>, false)`.
8. Jika return value dari fungsi `deleteTable` sama dengan `true`, tampilkan **Database dropped** pada client.

#### Fitur Drop Table
1. Pastikan client sedang menggunakan suatu database.
   * Jika tidak, tampilkan **Error::No database used** pada client.
2. Dapatkan tabel yang akan didrop dari client dan kirim ke server.
   * Dari perintah: `DROP TABLE <nama table>;`.
3. Pastikan tabel tersebut ada pada database yang sedang digunakan.
   * Jika tidak ada, tampilkan **Error::Table not found**.
4. Hapus table dengan perintah `remove("<curr_db>/<nama table>")`.
5. Tampilkan **Table dropped** pada client.

#### Fitur Drop Column
1. Pastikan client sedang menggunakan suatu database.
   * Jika tidak, tampilkan **Error::No database used** pada client.
2. Dapatkan nama kolom yang akan didrop beserta tabelnya dari client dan kirim ke server.
3. Pastikan tabel tersebut ada pada database yang sedang digunakan.
   * Dari perintah: `DROP COLUMN <kolom> FROM <table>;`.
   * Jika tidak ada, tampilkan **Error::Table not found**.
4. Hapus kolom pada tabel di baris tertentu dengan perintah `deleteTable(fd, NULL, <table>, <kolom>, NULL, false)`.
5. Jika return value dari fungsi `deleteTable` sama dengan `true`, tampilkan **Column dropped** pada client.