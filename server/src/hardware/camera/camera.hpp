/**
 * server/src/hardware/camera/camera.hpp
 *
 * This file is part of the traintastic source code.
 *
 * Copyright (C) 2025 Reinder Feenstra
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#ifndef TRAINTASTIC_SERVER_HARDWARE_CAMERA_CAMERA_HPP
#define TRAINTASTIC_SERVER_HARDWARE_CAMERA_CAMERA_HPP

#include "../../core/idobject.hpp"
#include "../../core/property.hpp"
#include <traintastic/enum/cameratype.hpp>
#include <traintastic/enum/cameraresolution.hpp>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <future>
#include <vector>
#include <string>
#include <string_view>
#include <functional>
#include <utility>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
  #define NOMINMAX
  #endif
  #include <windows.h>
#endif

class CameraList;
class CameraCapture;

class Camera : public IdObject
{
  CLASS_ID("camera")
  DEFAULT_ID("camera")
  CREATE(Camera)
  friend class CameraList;

  public:
    Property<std::string>  name;
    Property<CameraType>   type;
    Property<std::string>  device;
    Property<bool>         enabled;
    Property<std::string>  streamUrl;
    Property<uint32_t>     frameWidth;
    Property<uint32_t>     frameHeight;
    Property<double>       fps;
    Property<int>          jpegQuality; ///< 1-100, default 75
    Property<bool>         flipVertical;
    Property<bool>         flipHorizontal;
    Property<CameraResolution> resolution;  ///< requested capture size (local cameras)
    Property<int>          brightness;      ///< signed -100..100, 0 = neutral, negative darkens (best-effort)
    Property<bool>         autoBrightness;  ///< true = leave brightness at the camera default (slider disabled)
    Property<bool>         requestFromSource; ///< MJPEG: append fps/quality/res as URL query params

    using FrameCallback = std::function<void(std::vector<uint8_t> jpegData)>;
    /// Subscribe to encoded JPEG frames; the returned id is passed to
    /// removeFrameSubscriber() to unsubscribe. Any number of consumers may
    /// subscribe (stream connection, recorder, motion detection, ...).
    /// The callback is invoked on the capture thread, not the event loop: it
    /// must not block and should hand the data off (copy/move) and return
    /// promptly -- see CameraStreamConnection, which posts it to its own
    /// executor. Unsubscribing from within the callback is allowed.
    uint64_t addFrameSubscriber(FrameCallback cb);
    void     removeFrameSubscriber(uint64_t id);

    /// Human-readable device: the local camera's name for Local type, else the URL.
    std::string deviceDisplayName() const;

    Camera(World& world, std::string_view _id);
    ~Camera() override;

  protected:
    void addToWorld()  override;
    void loaded()      override;
    void destroying()  override;
    void worldEvent(WorldState state, WorldEvent event) override;

  private:
    std::unique_ptr<CameraCapture>                    m_capture;
    std::atomic<bool>                                 m_running{false};
  #ifdef _WIN32
    HANDLE                                            m_captureThreadHandle{nullptr};
  #else
    std::thread                                       m_captureThread;
  #endif
    std::mutex                                        m_subscriberMutex;
    std::vector<std::pair<uint64_t, FrameCallback>>  m_subscribers;
    uint64_t                                          m_nextSubscriberId{1};

    std::vector<std::string>       m_deviceValues;
    std::vector<std::string>       m_deviceNamesStr;
    std::vector<std::string_view>  m_deviceNames;
    std::vector<std::vector<std::pair<uint32_t, uint32_t>>> m_deviceResolutions; ///< declared per device (from the OS), largest-first
    std::vector<std::pair<uint32_t, uint32_t>> m_deviceNativeResolution;         ///< current/default ("native") size per device, {0,0} if unknown

    // Backing storage for the resolution property's aliases (tags the native size
    // in the dropdown, e.g. "1280 x 720 (720p) (native)"). The alias attribute
    // stores pointers to these, so they must outlive it -- mirrors the device
    // dropdown's m_deviceValues/m_deviceNames. m_resolutionAliasValues are views
    // into m_resolutionAliasValueStrings, so rebuild the strings first.
    std::vector<CameraResolution>   m_resolutionAliasKeys;
    std::vector<std::string>        m_resolutionAliasValueStrings;
    std::vector<std::string_view>   m_resolutionAliasValues;

    void updateSpecVisibility();
    void updateBrightnessEnabled();
    void updateResolutionValues();                     ///< offer only the selected camera's resolutions
    std::vector<std::pair<uint32_t, uint32_t>> deviceResolutions(const std::string& dev) const; ///< OS-declared sizes of a device, largest-first
    std::pair<uint32_t, uint32_t> deviceNativeResolution(const std::string& dev) const; ///< device's current/default size, {0,0} if unknown
    std::pair<uint32_t, uint32_t> autoResolution() const; ///< Auto: a single safe size to open (no probing)
    void startCapture();
    void stopCapture();
    void captureLoop();
    void captureLoopBody();
    void publishFrame(std::vector<uint8_t> jpegData);
    void applySettings();
    void updateDeviceAttribute();
};
#endif
