/**
 * server/src/hardware/3dSound/systemVolumeControl.hpp
 *
 * This file is part of the traintastic source code.
 *
 * Copyright (C) 2025 Reinder Feenstra
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef TRAINTASTIC_SERVER_HARDWARE_3DSOUND_SYSTEMVOLUMECONTROL_HPP
#define TRAINTASTIC_SERVER_HARDWARE_3DSOUND_SYSTEMVOLUMECONTROL_HPP

#include <string>
#include <memory>

/**
 * @brief Controls Windows system volume (master volume)
 * 
 * This class provides functionality to get and set the system-wide audio volume
 * on Windows. On non-Windows platforms, it provides stub implementations.
 */
class SystemVolumeControl
{
public:
  static SystemVolumeControl& instance();
  
  /**
   * @brief Get the current system volume
   * @return Volume level from 0.0 (muted) to 1.0 (maximum)
   */
  double getSystemVolume();
  
  /**
   * @brief Set the system volume
   * @param volume Volume level from 0.0 (muted) to 1.0 (maximum)
   * @return true on success, false on failure
   */
  bool setSystemVolume(double volume);
  
  /**
   * @brief Get the system mute state
   * @return true if system is muted, false otherwise
   */
  bool getSystemMute();
  
  /**
   * @brief Set the system mute state
   * @param muted true to mute, false to unmute
   * @return true on success, false on failure
   */
  bool setSystemMute(bool muted);
  
  /**
   * @brief Get volume for a specific audio device
   * @param deviceId The audio device ID (empty string for default device)
   * @return Volume level from 0.0 to 1.0, or -1.0 on error
   */
  double getDeviceVolume(const std::string& deviceId = "");
  
  /**
   * @brief Set volume for a specific audio device
   * @param deviceId The audio device ID (empty string for default device)
   * @param volume Volume level from 0.0 (muted) to 1.0 (maximum)
   * @return true on success, false on failure
   */
  bool setDeviceVolume(const std::string& deviceId, double volume);
  
private:
  SystemVolumeControl() = default;
  ~SystemVolumeControl();
  
  SystemVolumeControl(const SystemVolumeControl&) = delete;
  SystemVolumeControl& operator=(const SystemVolumeControl&) = delete;
  
#ifdef _WIN32
  struct Impl;
  std::unique_ptr<Impl> m_impl;
  
  bool ensureInitialized();
  void cleanup();
#endif
};

#endif
