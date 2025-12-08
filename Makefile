CC = gcc
CFLAGS = -Wall -I./src/common

# Tự động tạo thư mục bin và obj
$(shell mkdir -p bin obj/server obj/client obj/common)

# Danh sách các file nguồn
COMMON_SRC = src/common/network.c src/common/utils.c
SERVER_SRC = src/server/main.c src/server/transfer.c $(COMMON_SRC)
CLIENT_SRC = src/client/main.c src/client/file_handler.c $(COMMON_SRC)

# Tên file chạy output
SERVER_BIN = bin/server
CLIENT_BIN = bin/client

# --- PHẦN QUAN TRỌNG: KHAI BÁO PHONY ---
# .PHONY báo cho make biết "all", "clean" không phải là tên file
.PHONY: all clean

# Mục tiêu mặc định
all: $(SERVER_BIN) $(CLIENT_BIN)

# Quy tắc biên dịch Server
# Nó sẽ so sánh: nếu file nguồn mới hơn file bin/server thì mới biên dịch lại
$(SERVER_BIN): $(SERVER_SRC)
	$(CC) $(CFLAGS) $(SERVER_SRC) -o $(SERVER_BIN)

# Quy tắc biên dịch Client
$(CLIENT_BIN): $(CLIENT_SRC)
	$(CC) $(CFLAGS) $(CLIENT_SRC) -o $(CLIENT_BIN)

clean:
	rm -rf bin obj