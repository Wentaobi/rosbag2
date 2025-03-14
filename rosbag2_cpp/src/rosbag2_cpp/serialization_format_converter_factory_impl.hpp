// Copyright 2018 Open Source Robotics Foundation, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef ROSBAG2_CPP__SERIALIZATION_FORMAT_CONVERTER_FACTORY_IMPL_HPP_
#define ROSBAG2_CPP__SERIALIZATION_FORMAT_CONVERTER_FACTORY_IMPL_HPP_

#include "rosbag2_cpp/serialization_format_converter_factory_interface.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "pluginlib/class_loader.hpp"

#include "rosbag2_cpp/converter_interfaces/serialization_format_converter.hpp"
#include "rosbag2_cpp/logging.hpp"
#include "rosbag2_cpp/visibility_control.hpp"

#include "./rmw_implemented_serialization_format_converter.hpp"

namespace rosbag2_cpp
{

const char * converter_suffix = "_converter";

class SerializationFormatConverterFactoryImpl
{
public:
  SerializationFormatConverterFactoryImpl()
  {
    try {
      converter_class_loader_ =
        std::make_unique<pluginlib::ClassLoader<
            converter_interfaces::SerializationFormatConverter>>(
        "rosbag2_cpp",
        converter_interfaces::SerializationFormatConverter::get_base_class_name());
      serializer_class_loader_ =
        std::make_shared<pluginlib::ClassLoader<
            converter_interfaces::SerializationFormatSerializer>>(
        "rosbag2_cpp",
        converter_interfaces::SerializationFormatSerializer::get_base_class_name());
      deserializer_class_loader_ =
        std::make_shared<pluginlib::ClassLoader<
            converter_interfaces::SerializationFormatDeserializer>>(
        "rosbag2_cpp",
        converter_interfaces::SerializationFormatDeserializer::get_base_class_name());
    } catch (const std::exception & e) {
      ROSBAG2_CPP_LOG_ERROR_STREAM("Unable to create class loader instance: " << e.what());
      throw e;
    }
  }

  ~SerializationFormatConverterFactoryImpl() = default;

  std::unique_ptr<converter_interfaces::SerializationFormatDeserializer>
  load_deserializer(const std::string & format)
  {
    return load_interface(format, deserializer_class_loader_);
  }

  std::unique_ptr<converter_interfaces::SerializationFormatSerializer>
  load_serializer(const std::string & format)
  {
    return load_interface(format, serializer_class_loader_);
  }

  std::vector<std::string> get_declared_serialization_plugins() const
  {
    return serializer_class_loader_->getDeclaredClasses();
  }

private:
  // Return true if a plugin is found with this ID
  bool is_plugin_registered(
    const std::string & converter_id,
    const std::vector<std::string> & registered_converter_classes,
    const std::vector<std::string> & registered_interface_classes)
  {
    auto class_exists_in_converters = std::find(
      registered_converter_classes.begin(),
      registered_converter_classes.end(), converter_id);
    auto class_exists_in_deserializers = std::find(
      registered_interface_classes.begin(),
      registered_interface_classes.end(), converter_id);
    return !(class_exists_in_converters == registered_converter_classes.end() &&
           class_exists_in_deserializers == registered_interface_classes.end());
  }

  template<typename SerializationFormatIface>
  std::unique_ptr<SerializationFormatIface>
  load_interface(
    const std::string & format,
    std::shared_ptr<pluginlib::ClassLoader<SerializationFormatIface>> class_loader)
  {
    const auto converter_id = format + converter_suffix;
    if (is_plugin_registered(
        converter_id,
        converter_class_loader_->getDeclaredClasses(),
        class_loader->getDeclaredClasses()))
    {
      try {
        return std::unique_ptr<SerializationFormatIface>(
          class_loader->createUnmanagedInstance(converter_id));
      } catch (const std::runtime_error & ex) {
        (void) ex;
        // Plugin was not a SerializationFormatIface, try to load as SerializationFormatConverter.
      }

      try {
        return std::unique_ptr<converter_interfaces::SerializationFormatConverter>(
          converter_class_loader_->createUnmanagedInstance(converter_id));
      } catch (const std::runtime_error & ex) {
        ROSBAG2_CPP_LOG_ERROR_STREAM(
          "Unexpectedly unable to load instance of discovered converter interface: " <<
            ex.what() << ". Falling back to RMW implementation search.");
      }
    }

    ROSBAG2_CPP_LOG_INFO(
      "No plugin found providing serialization format. "
      "Falling back to checking RMW implementations.");
    try {
      return std::make_unique<RMWImplementedConverter>(format);
    } catch (const std::runtime_error & ex) {
      ROSBAG2_CPP_LOG_ERROR_STREAM("Could not initialize RMWImplementedConverter: " << ex.what());
    }
    return nullptr;
  }

  std::unique_ptr<
    pluginlib::ClassLoader<converter_interfaces::SerializationFormatConverter>>
  converter_class_loader_;
  std::shared_ptr<
    pluginlib::ClassLoader<converter_interfaces::SerializationFormatSerializer>>
  serializer_class_loader_;
  std::shared_ptr<
    pluginlib::ClassLoader<converter_interfaces::SerializationFormatDeserializer>>
  deserializer_class_loader_;
};

}  // namespace rosbag2_cpp

#endif  // ROSBAG2_CPP__SERIALIZATION_FORMAT_CONVERTER_FACTORY_IMPL_HPP_
