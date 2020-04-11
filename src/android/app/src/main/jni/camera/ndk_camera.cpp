// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <mutex>
#include <camera/NdkCameraCaptureSession.h>
#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraMetadata.h>
#include <camera/NdkCaptureRequest.h>
#include <libyuv.h>
#include <media/NdkImageReader.h>
#include "common/scope_exit.h"
#include "core/frontend/camera/blank_camera.h"
#include "jni/camera/ndk_camera.h"
#include "jni/id_cache.h"

namespace Camera::NDK {

/**
 * Implementation detail of NDK camera interface, holding a ton of different structs.
 * As long as the object lives, the camera is opened and capturing image. To turn off the camera,
 * one needs to destruct the object.
 * It captures at the maximum resolution supported by the device, capped at the 3DS camera's maximum
 * resolution (640x480). The pixel format is I420.
 */
class CaptureSession final {
public:
    explicit CaptureSession(ACameraManager* manager, const std::string& id);

private:
    std::pair<int, int> selected_resolution{};

    ACameraDevice_StateCallbacks device_callbacks{};
    AImageReader_ImageListener listener{};
    ACameraCaptureSession_stateCallbacks session_callbacks{};
    std::array<ACaptureRequest*, 1> requests{};

#define MEMBER(type, name, func)                                                                   \
    struct type##Deleter {                                                                         \
        void operator()(type* ptr) {                                                               \
            type##_##func(ptr);                                                                    \
        }                                                                                          \
    };                                                                                             \
    std::unique_ptr<type, type##Deleter> name

    MEMBER(ACameraDevice, device, close);
    MEMBER(AImageReader, image_reader, delete);

    // This window doesn't need to be destructed as it is managed by AImageReader
    ANativeWindow* native_window{};

    MEMBER(ACaptureSessionOutputContainer, outputs, free);
    MEMBER(ACaptureSessionOutput, output, free);
    MEMBER(ACameraOutputTarget, target, free);
    MEMBER(ACaptureRequest, request, free);

    // Put session last to close the session before we destruct everything else
    MEMBER(ACameraCaptureSession, session, close);
#undef MEMBER

    bool ready = false;

    std::mutex data_mutex;

    // Clang does not yet have shared_ptr to arrays support. Managed data are actually arrays.
    std::array<std::shared_ptr<u8>, 3> data; // I420 format, planes are Y, U, V.

    friend class Interface;
    friend void ImageCallback(void* context, AImageReader* reader);
};

static void OnDisconnected(void* context, ACameraDevice* device) {
    LOG_ERROR(Service_CAM, "Camera device disconnected");
    // TODO: Do something here?
    // CaptureSession* that = reinterpret_cast<CaptureSession*>(context);
    // that->CloseCamera();
}

static void OnError(void* context, ACameraDevice* device, int error) {
    LOG_ERROR(Service_CAM, "Camera device error {}", error);
    // TODO: Do something here?
    // CaptureSession* that = reinterpret_cast<CaptureSession*>(context);
    // that->CloseCamera();
}

#define MEDIA_CALL(func)                                                                           \
    {                                                                                              \
        auto ret = func;                                                                           \
        if (ret != AMEDIA_OK) {                                                                    \
            LOG_ERROR(Service_CAM, "Call " #func " returned error {}", ret);                       \
            return;                                                                                \
        }                                                                                          \
    }

#define CAMERA_CALL(func)                                                                          \
    {                                                                                              \
        auto ret = func;                                                                           \
        if (ret != ACAMERA_OK) {                                                                   \
            LOG_ERROR(Service_CAM, "Call " #func " returned error {}", ret);                       \
            return;                                                                                \
        }                                                                                          \
    }

void ImageCallback(void* context, AImageReader* reader) {
    AImage* image{};
    MEDIA_CALL(AImageReader_acquireLatestImage(reader, &image))
    SCOPE_EXIT({ AImage_delete(image); });

    std::array<std::shared_ptr<u8>, 3> data;
    for (const int plane : {0, 1, 2}) {
        u8* ptr;
        int size;
        MEDIA_CALL(AImage_getPlaneData(image, plane, &ptr, &size));
        data[plane].reset(new u8[size], std::default_delete<u8[]>());
        std::memcpy(data[plane].get(), ptr, static_cast<size_t>(size));
    }

    {
        CaptureSession* that = reinterpret_cast<CaptureSession*>(context);
        std::lock_guard lock{that->data_mutex};
        that->data = data;
    }
}

#define CREATE(type, name, statement)                                                              \
    {                                                                                              \
        type* raw;                                                                                 \
        statement;                                                                                 \
        name.reset(raw);                                                                           \
    }

CaptureSession::CaptureSession(ACameraManager* manager, const std::string& id) {
    device_callbacks = {
        /*context*/ nullptr,
        /*onDisconnected*/ &OnDisconnected,
        /*onError*/ &OnError,
    };

    CREATE(ACameraDevice, device,
           CAMERA_CALL(ACameraManager_openCamera(manager, id.c_str(), &device_callbacks, &raw)));

    ACameraMetadata* metadata;
    CAMERA_CALL(ACameraManager_getCameraCharacteristics(manager, id.c_str(), &metadata));

    ACameraMetadata_const_entry entry;
    CAMERA_CALL(ACameraMetadata_getConstEntry(
        metadata, ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS, &entry));
    selected_resolution = {};
    for (std::size_t i = 0; i < entry.count; i += 4) {
        // (format, width, height, input?)
        if (entry.data.i32[i + 3] & ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_INPUT) {
            // This is an input stream
            continue;
        }

        int format = entry.data.i32[i + 0];
        if (format == AIMAGE_FORMAT_YUV_420_888) {
            int width = entry.data.i32[i + 1];
            int height = entry.data.i32[i + 2];
            if (width <= 640 && height <= 480) { // Maximum size the 3DS supports
                selected_resolution = std::max(selected_resolution, std::make_pair(width, height));
            }
        }
    }
    ACameraMetadata_free(metadata);

    if (selected_resolution == std::pair<int, int>{}) {
        LOG_ERROR(Service_CAM, "Device does not support any YUV output format");
        return;
    }

    CREATE(AImageReader, image_reader,
           MEDIA_CALL(AImageReader_new(selected_resolution.first, selected_resolution.second,
                                       AIMAGE_FORMAT_YUV_420_888, 4, &raw)));

    listener = {
        /*context*/ this,
        /*onImageAvailable*/ &ImageCallback,
    };
    MEDIA_CALL(AImageReader_setImageListener(image_reader.get(), &listener));

    MEDIA_CALL(AImageReader_getWindow(image_reader.get(), &native_window));
    CREATE(ACaptureSessionOutput, output,
           CAMERA_CALL(ACaptureSessionOutput_create(native_window, &raw)));

    CREATE(ACaptureSessionOutputContainer, outputs,
           CAMERA_CALL(ACaptureSessionOutputContainer_create(&raw)));
    CAMERA_CALL(ACaptureSessionOutputContainer_add(outputs.get(), output.get()));

    CREATE(ACameraCaptureSession, session,
           CAMERA_CALL(ACameraDevice_createCaptureSession(device.get(), outputs.get(),
                                                          &session_callbacks, &raw)));
    CREATE(ACaptureRequest, request,
           CAMERA_CALL(ACameraDevice_createCaptureRequest(device.get(), TEMPLATE_PREVIEW, &raw)));

    ANativeWindow_acquire(native_window);
    CREATE(ACameraOutputTarget, target,
           CAMERA_CALL(ACameraOutputTarget_create(native_window, &raw)));
    CAMERA_CALL(ACaptureRequest_addTarget(request.get(), target.get()));

    requests = {request.get()};
    CAMERA_CALL(ACameraCaptureSession_setRepeatingRequest(session.get(), nullptr, 1,
                                                          requests.data(), nullptr));

    ready = true;
}

#undef MEDIA_CALL
#undef CAMERA_CALL
#undef CREATE

Interface::Interface(Factory& factory_, const std::string& id_, const Service::CAM::Flip& flip_)
    : factory(factory_), id(id_), flip(flip_) {}

Interface::~Interface() = default;

void Interface::StartCapture() {
    session = factory.CreateCaptureSession(id);
}

void Interface::StopCapture() {
    session.reset();
}

void Interface::SetResolution(const Service::CAM::Resolution& resolution_) {
    resolution = resolution_;
}

void Interface::SetFlip(Service::CAM::Flip flip_) {
    flip = flip_;
}

void Interface::SetFormat(Service::CAM::OutputFormat format_) {
    format = format_;
}

std::vector<u16> Interface::ReceiveFrame() {
    std::array<std::shared_ptr<u8>, 3> data;
    {
        std::lock_guard lock{session->data_mutex};
        data = session->data;
    }

    auto [width, height] = session->selected_resolution;

    int crop_width{}, crop_height{};
    if (resolution.width * height > resolution.height * width) {
        crop_width = width;
        crop_height = width * resolution.height / resolution.width;
    } else {
        crop_height = height;
        crop_width = height * resolution.width / resolution.height;
    }

    int crop_x = (width - crop_width) / 2;
    int crop_y = (height - crop_height) / 2;
    int offset = crop_y * width + crop_x;
    std::vector<u8> scaled_y(resolution.width * resolution.height);
    std::vector<u8> scaled_u(resolution.width * resolution.height / 4ul);
    std::vector<u8> scaled_v(resolution.width * resolution.height / 4ul);
    // Crop and scale
    libyuv::I420Scale(data[0].get() + offset, width, data[1].get() + offset / 4, width / 4,
                      data[2].get() + offset / 4, width / 4, crop_width, crop_height,
                      scaled_y.data(), resolution.width, scaled_u.data(), resolution.width / 4,
                      scaled_v.data(), resolution.width / 4, resolution.width, resolution.height,
                      libyuv::kFilterBilinear);
    // TODO: Record and apply flip

    std::vector<u16> output(resolution.width * resolution.height);
    if (format == Service::CAM::OutputFormat::RGB565) {
        libyuv::I420ToRGB565(scaled_y.data(), resolution.width, scaled_u.data(),
                             resolution.width / 4, scaled_v.data(), resolution.width / 4,
                             reinterpret_cast<u8*>(output.data()), resolution.width * 2,
                             resolution.width, resolution.height);
    } else {
        libyuv::I420ToYUY2(scaled_y.data(), resolution.width, scaled_u.data(), resolution.width / 4,
                           scaled_v.data(), resolution.width / 4,
                           reinterpret_cast<u8*>(output.data()), resolution.width * 2,
                           resolution.width, resolution.height);
    }
    return output;
}

bool Interface::IsPreviewAvailable() {
    return session && session->ready;
}

Factory::Factory() = default;

Factory::~Factory() = default;

std::shared_ptr<CaptureSession> Factory::CreateCaptureSession(const std::string& id) {
    if (opened_camera_map.count(id) && !opened_camera_map.at(id).expired()) {
        return opened_camera_map.at(id).lock();
    }
    const auto& session = std::make_shared<CaptureSession>(manager.get(), id);
    opened_camera_map.insert_or_assign(id, session);
    return session;
}

std::unique_ptr<CameraInterface> Factory::Create(const std::string& config,
                                                 const Service::CAM::Flip& flip) {

    if (!manager) {
        JNIEnv* env = IDCache::GetEnvForThread();
        jboolean result = env->CallStaticBooleanMethod(IDCache::GetNativeLibraryClass(),
                                                       IDCache::GetRequestCameraPermission());
        if (result != JNI_TRUE) {
            LOG_ERROR(Service_CAM, "Camera permissions denied");
            return std::make_unique<Camera::BlankCamera>();
        }

        manager.reset(ACameraManager_create());
    }
    ACameraIdList* id_list = nullptr;

    auto ret = ACameraManager_getCameraIdList(manager.get(), &id_list);
    if (ret != ACAMERA_OK) {
        LOG_ERROR(Service_CAM, "Failed to get camera ID list: ret {}", ret);
        return std::make_unique<Camera::BlankCamera>();
    }

    SCOPE_EXIT({ ACameraManager_deleteCameraIdList(id_list); });

    if (id_list->numCameras <= 0) {
        LOG_ERROR(Service_CAM, "No camera devices found");
        return std::make_unique<Camera::BlankCamera>();
    }
    if (config.empty()) {
        LOG_WARNING(Service_CAM, "Camera ID not set, using default camera");
        return std::make_unique<Interface>(*this, id_list->cameraIds[0], flip);
    }

    for (int i = 0; i < id_list->numCameras; ++i) {
        const char* id = id_list->cameraIds[i];
        if (config == id) {
            return std::make_unique<Interface>(*this, id, flip);
        }
    }

    LOG_ERROR(Service_CAM, "Camera ID {} not found", config);
    return std::make_unique<Camera::BlankCamera>();
}

} // namespace Camera::NDK
