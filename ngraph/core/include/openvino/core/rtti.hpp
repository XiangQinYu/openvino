// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include "openvino/core/type.hpp"

#define _OPENVINO_RTTI_EXPAND(X)                                      X
#define _OPENVINO_RTTI_DEFINITION_SELECTOR(_1, _2, _3, _4, NAME, ...) NAME

#define _OPENVINO_RTTI_WITH_TYPE(TYPE_NAME) _OPENVINO_RTTI_WITH_TYPE_VERSION(TYPE_NAME, "util")

#define _OPENVINO_RTTI_WITH_TYPE_VERSION(TYPE_NAME, VERSION_NAME)                  \
    static const ::ov::DiscreteTypeInfo& get_type_info_static() {                  \
        static const ::ov::DiscreteTypeInfo type_info{TYPE_NAME, 0, VERSION_NAME}; \
        return type_info;                                                          \
    }                                                                              \
    const ::ov::DiscreteTypeInfo& get_type_info() const override {                 \
        return get_type_info_static();                                             \
    }

#define _OPENVINO_RTTI_WITH_TYPE_VERSION_PARENT(TYPE_NAME, VERSION_NAME, PARENT_CLASS) \
    _OPENVINO_RTTI_WITH_TYPE_VERSIONS_PARENT(TYPE_NAME, VERSION_NAME, PARENT_CLASS, 0)

#define _OPENVINO_RTTI_WITH_TYPE_VERSIONS_PARENT(TYPE_NAME, VERSION_NAME, PARENT_CLASS, OLD_VERSION) \
    static const ::ov::DiscreteTypeInfo& get_type_info_static() {                                    \
        static const ::ov::DiscreteTypeInfo type_info{TYPE_NAME,                                     \
                                                      OLD_VERSION,                                   \
                                                      VERSION_NAME,                                  \
                                                      &PARENT_CLASS::get_type_info_static()};        \
        return type_info;                                                                            \
    }                                                                                                \
    const ::ov::DiscreteTypeInfo& get_type_info() const override {                                   \
        return get_type_info_static();                                                               \
    }
/// Helper macro that puts necessary declarations of RTTI block inside a class definition.
/// Should be used in the scope of class that requires type identification besides one provided by
/// C++ RTTI.
/// Recommended to be used for all classes that are inherited from class ov::Node to enable
/// pattern
/// matching for them. Accepts necessary type identification details like type of the operation,
/// version and optional parent class.
///
/// Applying this macro within a class definition provides declaration of type_info static
/// constant for backward compatibility with old RTTI definition for Node,
/// static function get_type_info_static which returns a reference to an object that is equal to
/// type_info but not necessary to the same object, and get_type_info virtual function that
/// overrides Node::get_type_info and returns a reference to the same object that
/// get_type_info_static gives.
///
/// Use this macro as a public part of the class definition:
///
///     class MyClass
///     {
///         public:
///             OPENVINO_RTTI("MyClass", "my_version");
///
///             ...
///     };
///
///     class MyClass2: public MyClass
///     {
///         public:
///             OPENVINO_RTTI("MyClass2", "my_version2", MyClass);
///
///             ...
///     };
///
/// \param TYPE_NAME a string literal of type const char* that names your class in type
/// identification namespace;
///        It is your choice how to name it, but it should be unique among all
///        OPENVINO_RTTI_DECLARATION-enabled classes that can be
///        used in conjunction with each other in one transformation flow.
/// \param VERSION_NAME is an name of operation version to distinguish different versions of
///        operations that shares the same TYPE_NAME
/// \param PARENT_CLASS is an optional direct or indirect parent class for this class; define
///        it only in case if there is a need to capture any operation from some group of operations
///        that all derived from some common base class. Don't use Node as a parent, it is a base
///        class
///        for all operations and doesn't provide ability to define some perfect subset of
///        operations. PARENT_CLASS should define RTTI with OPENVINO_RTTI_{DECLARATION/DEFINITION}
///        macros.
/// \param _VERSION_INDEX is an unsigned integer index to distinguish different versions of
///        operations that shares the same TYPE_NAME (for backward compatibility)
///
/// OPENVINO_RTTI(name)
/// OPENVINO_RTTI(name, version_id)
/// OPENVINO_RTTI(name, version_id, parent)
/// OPENVINO_RTTI(name, version_id, parent, old_version)
#define OPENVINO_RTTI(...)                                                                             \
    _OPENVINO_RTTI_EXPAND(_OPENVINO_RTTI_DEFINITION_SELECTOR(__VA_ARGS__,                              \
                                                             _OPENVINO_RTTI_WITH_TYPE_VERSIONS_PARENT, \
                                                             _OPENVINO_RTTI_WITH_TYPE_VERSION_PARENT,  \
                                                             _OPENVINO_RTTI_WITH_TYPE_VERSION,         \
                                                             _OPENVINO_RTTI_WITH_TYPE)(__VA_ARGS__))

/// Note: Please don't use this macros for new operations
#ifdef OPENVINO_STATIC_LIBRARY
#    define BWDCMP_RTTI_DECLARATION
#    define BWDCMP_RTTI_DEFINITION(CLASS)
#else
#    define BWDCMP_RTTI_DECLARATION                                                                     \
        OPENVINO_DEPRECATED("This member was deprecated. Please use ::get_type_info_static() instead.") \
        static const ov::DiscreteTypeInfo type_info
#    define BWDCMP_RTTI_DEFINITION(CLASS)                                            \
        OPENVINO_SUPPRESS_DEPRECATED_START                                           \
        const ov::DiscreteTypeInfo CLASS::type_info = CLASS::get_type_info_static(); \
        OPENVINO_SUPPRESS_DEPRECATED_END
#endif
