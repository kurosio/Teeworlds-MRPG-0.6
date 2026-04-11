# MariaDB for MRPG: Configuration and Recommendations

## 1) Recommended MariaDB settings 

```ini
[mysqld]
# Basic Network Settings
bind-address = 127.0.0.1
port = 3306
skip-name-resolve = 1

# Encoding
character-set-server = utf8mb4
collation-server = utf8mb4_unicode_ci

# InnoDB (profile for a game server)
innodb_buffer_pool_size = 1G
innodb_log_file_size = 256M
innodb_flush_log_at_trx_commit = 2
innodb_flush_method = O_DIRECT
innodb_file_per_table = 1

# Connections
max_connections = 200
thread_cache_size = 100

# Timetables / Schedules
tmp_table_size = 64M
max_heap_table_size = 64M
sort_buffer_size = 2M
join_buffer_size = 2M

# Logs
slow_query_log = 1
slow_query_log_file = /var/log/mysql/mariadb-slow.log
long_query_time = 1
```

## 2) MariaDB configuration application commands (Ubuntu, Debian)

```bash
# 1. Create a separate configuration for game mode
sudo tee /etc/mysql/mariadb.conf.d/60-mrpg.cnf >/dev/null <<'CNF'
[mysqld]
bind-address = 127.0.0.1
port = 3306
skip-name-resolve = 1
character-set-server = utf8mb4
collation-server = utf8mb4_unicode_ci
innodb_buffer_pool_size = 1G
innodb_log_file_size = 256M
innodb_flush_log_at_trx_commit = 2
innodb_flush_method = O_DIRECT
innodb_file_per_table = 1
max_connections = 200
thread_cache_size = 100
tmp_table_size = 64M
max_heap_table_size = 64M
sort_buffer_size = 2M
join_buffer_size = 2M
slow_query_log = 1
slow_query_log_file = /var/log/mysql/mariadb-slow.log
long_query_time = 1
CNF

# 2. Check the syntax and restart
sudo mariadbd --help >/dev/null
sudo systemctl restart mariadb
sudo systemctl status mariadb --no-pager

# 3. Check the values used
mysql -u root -p -e "SHOW VARIABLES LIKE 'innodb_buffer_pool_size';"
mysql -u root -p -e "SHOW VARIABLES LIKE 'max_connections';"
mysql -u root -p -e "SHOW VARIABLES LIKE 'character_set_server';"
```

## 3) Database and user commands for MRPG

```sql
CREATE DATABASE IF NOT EXISTS teeworlds_mrpg
  CHARACTER SET utf8mb4
  COLLATE utf8mb4_unicode_ci;

CREATE USER IF NOT EXISTS 'mrpg_user'@'127.0.0.1' IDENTIFIED BY 'CHANGE_ME_STRONG_PASSWORD';
GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, ALTER, INDEX ON teeworlds_mrpg.* TO 'mrpg_user'@'127.0.0.1';
FLUSH PRIVILEGES;
```

Importing database:

```bash
mysql -u root -p teeworlds_mrpg < mmprpg_clean.sql
```
