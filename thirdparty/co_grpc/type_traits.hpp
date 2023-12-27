#pragma once

namespace co_grpc {

    template<typename T>
    struct type_traits
    {
        typedef T         value_type;
        typedef const T   const_value_type;
        typedef T*        pointer_type;
        typedef const T*  const_pointer_type;
        typedef T**       pointer_pointer_type;
        typedef const T** const_pointer_pointer_type;
        typedef T&        reference_type;
        typedef const T&  const_reference_type;
        typedef T*&       pointer_reference_type;
        typedef const T*& const_pointer_reference_type;

        static bool is_reference(){return false;};
        static bool is_pointer(){return false;};
        static bool is_value(){return true;};
        static bool is_pointer_reference(){return false;}
        static bool is_pointer_pointer(){return false;}

        static bool is_const_reference(){return false;};
        static bool is_const_pointer(){return false;};
        static bool is_const_value(){return false;};
        static bool is_const_pointer_reference(){return false;}
        static bool is_const_pointer_pointer(){return false;}
    };

    template<typename T>
    struct type_traits<const T> : public type_traits<T>
    {
        static bool is_const_reference(){return false;};
        static bool is_const_pointer(){return false;};
        static bool is_const_value(){return true;};
        static bool is_const_pointer_reference(){return false;}
        static bool is_const_pointer_pointer(){return false;}
    };

    template<typename T>
    struct type_traits<T*>: public type_traits<T>
    {
        static bool is_reference(){return false;}
        static bool is_pointer(){return true;}
        static bool is_value(){return false;}
        static bool is_pointer_reference(){return false;}
        static bool is_pointer_pointer(){return false;}
    };

    template<typename T>
    struct type_traits<const T*>: public type_traits<T*>
    {
        static bool is_const_reference(){return false;};
        static bool is_const_pointer(){return true;};
        static bool is_const_value(){return false;};
        static bool is_const_pointer_reference(){return false;}
        static bool is_const_pointer_pointer(){return false;}
    };

    template<typename T>
    struct type_traits<T&>: public type_traits<T>
    {
        static bool is_reference(){return true;}
        static bool is_pointer(){return false;}
        static bool is_value(){return false;}
        static bool is_pointer_reference(){return false;}
        static bool is_pointer_pointer(){return false;}
    };

    template<typename T>
    struct type_traits<const T&>: public type_traits<T&>
    {
        static bool is_const_reference(){return true;};
        static bool is_const_pointer(){return false;};
        static bool is_const_value(){return false;};
        static bool is_const_pointer_reference(){return false;}
        static bool is_const_pointer_pointer(){return false;}
    };

    template<typename T>
    struct type_traits<T**>: public type_traits<T>
    {
        static bool is_reference(){return false;}
        static bool is_pointer(){return false;}
        static bool is_value(){return false;}
        static bool is_pointer_reference(){return false;}
        static bool is_pointer_pointer(){return true;}
    };

    template<typename T>
    struct type_traits<const T**>: public type_traits<T**>
    {
        static bool is_const_reference(){return false;};
        static bool is_const_pointer(){return false;};
        static bool is_const_value(){return false;};
        static bool is_const_pointer_reference(){return false;}
        static bool is_const_pointer_pointer(){return true;}
    };

    template<typename T>
    struct type_traits<T*&>: public type_traits<T>
    {
        static bool is_reference(){return false;}
        static bool is_pointer(){return false;}
        static bool is_value(){return false;}
        static bool is_pointer_reference(){return true;}
        static bool is_pointer_pointer(){return false;}
    };

    template<typename T>
    struct type_traits<const T*&>: public type_traits<T*&>
    {
        static bool is_const_reference(){return false;};
        static bool is_const_pointer(){return false;};
        static bool is_const_value(){return false;};
        static bool is_const_pointer_reference(){return true;}
        static bool is_const_pointer_pointer(){return false;}
    };

}

