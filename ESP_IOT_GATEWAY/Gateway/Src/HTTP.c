#include "global.h"

struct NetworkContext {
    int tcp_socket; 
};

static const char *TAG = "CORE_HTTP";

// Kích thước bộ đệm để chứa Header và Response (coreHTTP bắt buộc bạn tự cấp phát)
#define USER_BUFFER_LENGTH 10240

// --- BƯỚC 1: ĐỊNH NGHĨA "CẦU NỐI" (TRANSPORT INTERFACE) ---

// Hàm gửi dữ liệu (Bọc hàm send() của TCP)
static int32_t transport_send(NetworkContext_t * pContext, const void * pBuffer, size_t bytesToSend) {
    if (pContext == NULL || pContext->tcp_socket < 0) return -1;
    
    // Gọi hàm send y hệt như trong file TCP_IP.c của bạn
    int bytesSent = send(pContext->tcp_socket, pBuffer, bytesToSend, 0);
    return bytesSent;
}

// Hàm nhận dữ liệu (Bọc hàm recv() của TCP)
static int32_t transport_recv(NetworkContext_t * pContext, void * pBuffer, size_t bytesToRecv) {
    if (pContext == NULL || pContext->tcp_socket < 0) return -1;
    
    // Gọi hàm recv y hệt như trong file TCP_IP.c của bạn
    int bytesReceived = recv(pContext->tcp_socket, pBuffer, bytesToRecv, 0);
    return bytesReceived;
}


// --- BƯỚC 2: TASK XỬ LÝ CHÍNH ---

void core_http_task(void *pvParameters) {
    int sock = -1;
    char *userBuffer = malloc(USER_BUFFER_LENGTH); // Tạo bộ đệm
    
    while(1) {
        // ---------------------------------------------------------
        // GIAI ĐOẠN A: MỞ SOCKET TCP (Y hệt TCP_IP.c)
        // ---------------------------------------------------------
        struct hostent *hp = gethostbyname("httpforever.com");
        if (hp == NULL) {
            ESP_LOGE(TAG, "Phân giải DNS thất bại! Vui lòng kiểm tra lại mạng.");
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            continue;
        }
        struct sockaddr_in dest_addr;
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(80); // Cổng 80 cho HTTP (Blynk hỗ trợ)
        dest_addr.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;

        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (sock < 0) {
            ESP_LOGE(TAG, "Lỗi tạo socket");
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            continue;
        }

        if (connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0) {
            ESP_LOGE(TAG, "Lỗi kết nối TCP");
            close(sock);
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "Đã mở TCP Socket thành công!");

        // ---------------------------------------------------------
        // GIAI ĐOẠN B: SỬ DỤNG coreHTTP
        // ---------------------------------------------------------
        
        // 1. Gắn "Cầu nối" vào thư viện
        struct NetworkContext networkContext;
        networkContext.tcp_socket = sock;

        TransportInterface_t transport;
        transport.pNetworkContext = &networkContext;
        transport.send = transport_send;
        transport.recv = transport_recv;

        // 2. Thiết lập thông tin gói tin HTTP GET
        HTTPRequestInfo_t requestInfo = {0};
        requestInfo.pMethod = "GET";
        requestInfo.methodLen = strlen("GET");
        requestInfo.pHost = "httpforever.com";
        requestInfo.hostLen = strlen("httpforever.com");
        requestInfo.pPath = "/";
        requestInfo.pathLen = strlen("/");

        // 3. Khởi tạo Header vào bộ đệm của chúng ta
        HTTPRequestHeaders_t requestHeaders = {0};
        requestHeaders.pBuffer = (uint8_t *)userBuffer;
        requestHeaders.bufferLen = USER_BUFFER_LENGTH;
        
        HTTPClient_InitializeRequestHeaders(&requestHeaders, &requestInfo);

        // 4. Gửi và Nhận phản hồi
        HTTPResponse_t response = {0};
        response.pBuffer = (uint8_t *)userBuffer;
        response.bufferLen = USER_BUFFER_LENGTH;

        ESP_LOGI(TAG, "Đang gửi HTTP Request...");
        HTTPStatus_t httpStatus = HTTPClient_Send(&transport, &requestHeaders, NULL, 0, &response, 0);

        if (httpStatus == HTTPSuccess) {
            ESP_LOGI(TAG, "Thành công! Mã trạng thái Server: %u", response.statusCode);
            // In nội dung Server trả về
            if (response.pBody != NULL) {
                ESP_LOGI(TAG, "Nội dung: %.*s", (int)response.bodyLen, (char *)response.pBody);
            }
        } else {
            ESP_LOGE(TAG, "Gửi HTTP thất bại, mã lỗi coreHTTP: %d", httpStatus);
        }

        // ---------------------------------------------------------
        // GIAI ĐOẠN C: ĐÓNG CỬA VÀ ĐỢI
        // ---------------------------------------------------------
        close(sock); // Đóng TCP socket
        vTaskDelay(10000 / portTICK_PERIOD_MS); // Gửi 10 giây 1 lần
    }
}