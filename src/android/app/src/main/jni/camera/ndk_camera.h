// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <unordered_map>
#include <camera/NdkCameraManager.h>
#include "common/common_types.h"
#include "core/frontend/camera/factory.h"
#include "core/frontend/camera/interface.h"

namespace Camera::NDK {

class CaptureSession;
class Factory;

class Interface : public CameraInterface {
public:
    Interface(Factory& factory, const std::string& id, const Service::CAM::Flip& flip);
    ~Interface() override;
    void StartCapture() override;
    void StopCapture() override;
    void SetResolution(const Service::CAM::Resolution& resolution) override;
    void SetFlip(Service::CAM::Flip flip) override;
    void SetEffect(Service::CAM::Effect effect) override{};
    void SetFormat(Service::CAM::OutputFormat format) override;
    void SetFrameRate(Service::CAM::FrameRate frame_rate) override{};
    std::vector<u16> ReceiveFrame() override;
    bool IsPreviewAvailable() override;

private:
    // jstring path;
    Factory& factory;
    std::shared_ptr<CaptureSession> session;
    std::string id;

    Service::CAM::Resolution resolution;

    // Flipping parameters. mirror = horizontal, invert = vertical.
    bool base_mirror{};
    bool base_invert{};
    bool mirror{};
    bool invert{};

    Service::CAM::OutputFormat format;
    // std::vector<u16> image; // Data fetched from the frontend
    // bool opened{};          // Whether the camera was successfully opened
};

class Factory final : public CameraFactory {
public:
    explicit Factory();
    ~Factory() override;

    std::unique_ptr<CameraInterface> Create(const std::string& config,
                                            const Service::CAM::Flip& flip) override;

private:
    std::shared_ptr<CaptureSession> CreateCaptureSession(const std::string& id);

    // The session is cached, to avoid opening the same camera twice.
    // This is weak_ptr so that the session is destructed when all cameras are closed
    std::unordered_map<std::string, std::weak_ptr<CaptureSession>> opened_camera_map;

    struct ACameraManagerDeleter {
        void operator()(ACameraManager* manager) {
            ACameraManager_delete(manager);
        }
    };
    std::unique_ptr<ACameraManager, ACameraManagerDeleter> manager;

    friend class Interface;
};

// Device rotation. Updated in native.cpp.
inline int g_rotation = 0;

} // namespace Camera::NDK
