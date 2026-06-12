# Tự động quét và lấy toàn bộ file .c trong thư mục source
file(GLOB srcs "source/*.c")

# Khai báo các thư mục chứa file header
set(include_dirs 
    "source/include"
    "source/interface"
)

# Đăng ký component
idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "${include_dirs}")

# Cấu hình bỏ qua file config tùy chỉnh
target_compile_definitions(${COMPONENT_TARGET} PUBLIC CORE_MQTT_DO_NOT_USE_CUSTOM_CONFIG)