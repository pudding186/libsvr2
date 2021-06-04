#pragma once
#include <cstring>
#include <string>
#include <vector>
#include <bitset>
#include "./lib_svr_def.h"
#include "./smemory.hpp"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "rapidjson/reader.h"

#ifdef  __cplusplus
template< typename T, typename U = size_t, size_t N = 254/sizeof(T)+1, bool is_pod = std::is_pod<T>::value, bool is_unsigned = std::is_unsigned<U>::value >
class DataArray
{

};

template <typename T, typename U, size_t N>
class DataArray<T, U, N, true, true>
{
public:
    DataArray(void)
    {
        m_size = 0;
        m_capacity = static_cast<U>(N);
        m_array = m_cache;
    }

    ~DataArray()
    {
        if (m_array != m_cache)
        {
            S_FREE(m_array);
        }

        m_array = 0;
        m_size = 0;
        m_capacity = 0;
    }

    DataArray(const DataArray& src)
    {
        if (src.m_size <= static_cast<U>(N))
        {
            m_capacity = static_cast<U>(N);
            m_size = src.m_size;
            m_array = m_cache;
        }
        else
        {
            m_capacity = src.m_capacity;
            m_size = src.m_size;
            m_array = (T*)S_MALLOC_EX(sizeof(T) * m_capacity, m_name);
        }

        memcpy(m_array, src.m_array, sizeof(T) * m_size);
    }

    DataArray& operator= (const DataArray& src)
    {
        if (m_capacity < src.m_size)
        {
            m_capacity = src.m_capacity;

            if (m_array != m_cache)
            {
                S_FREE(m_array);
            }

            m_array = (T*)S_MALLOC_EX(sizeof(T) * m_capacity, m_name);
        }

        m_size = src.m_size;

        memcpy(m_array, src.m_array, sizeof(T)*m_size);

        return *this;
    }

    bool operator== (const DataArray& src) const 
    {
        if (src.m_size != m_size)
        {
            return false;
        }

        return (0 == memcmp(m_array, src.m_array, sizeof(T)*m_size));
    }

    bool operator != (const DataArray& src) const
    {
        return !(*this == src);
    }

    T& operator[](U idx) const
    {
        if (idx < m_size)
        {
            return m_array[idx];
        }
        else
        {
            throw "operator [] overflow";
            //return m_array[0];
        }
    }

    void resize(U new_size)
    {
        if (new_size > m_capacity)
        {
            m_capacity = new_size;

            if (m_array != m_cache)
            {
                m_array = (T*)S_REALLOC_EX(m_array, sizeof(T) * m_capacity, m_name);
            }
            else
            {
                m_array = (T*)S_MALLOC_EX(sizeof(T) * m_capacity, m_name);
                memcpy(m_array, m_cache, sizeof(T) * m_size);
            }

        }
        m_size = new_size;
    }

    void reserve(U new_size)
    {
        if (new_size > m_capacity)
        {
            m_capacity = new_size;

            if (m_array != m_cache)
            {
                m_array = (T*)S_REALLOC_EX(m_array, sizeof(T) * m_capacity, m_name);
            }
            else
            {
                m_array = (T*)S_MALLOC_EX(sizeof(T) * m_capacity, m_name);
                memcpy(m_array, m_cache, sizeof(T) * m_size);
            }
        }
    }

    void push_back(const T& val)
    {
        if (m_size < m_capacity)
        {
            memcpy(m_array + m_size, &val, sizeof(T));
            m_size++;
        }
        else
        {
            if ((std::numeric_limits<U>::max)() - m_capacity < m_capacity)
            {
                if (m_capacity == (std::numeric_limits<U>::max)())
                {
                    //CRUSH_CODE();
                    throw "capacity is max";
                }
                else
                {
                    m_capacity = (std::numeric_limits<U>::max)();
                }
            }
            else
            {
                m_capacity += m_capacity;
            }

            if (m_array != m_cache)
            {
                m_array = (T*)S_REALLOC_EX(m_array, sizeof(T) * m_capacity, m_name);
            }
            else
            {
                m_array = (T*)S_MALLOC_EX(sizeof(T) * m_capacity, m_name);
                memcpy(m_array, m_cache, sizeof(T) * m_size);
            }
            
            memcpy(m_array + m_size, &val, sizeof(T));
            m_size++;
        }
    }

    inline void erase(U idx)
    {
        if (idx < m_size)
        {
            size_t copy_len = sizeof(T) * (m_size - idx - 1);

            if (copy_len)
            {
                memcpy(m_array + idx, m_array + idx + 1, copy_len);
            }

            --m_size;
        }
        else
        {
            throw "operator erase overflow";
        }
    }

    inline void clear()
    {
        m_size = 0;
    }

    inline U size(void) const
    {
        return m_size;
    }

	inline size_t size_of_data(void) const
	{
		return sizeof(T);
	}

    inline U capacity(void) const
    {
        return m_capacity;
    }

	inline T* data(void) const
	{
		return m_array;
	}

    template <U S>
    inline void append_data(T(&datas)[S])
    {
        if ((std::numeric_limits<U>::max)() - m_size < S)
        {
            throw "capacity is max";
        }

        if (m_size + S > m_capacity)
        {
            reserve(m_size + S);
        }

        memcpy(m_array+m_size, datas, S*sizeof(T));
        m_size += S;
    }

    inline void append_data(const T *datas, U length)
    {
        if ((std::numeric_limits<U>::max)() - m_size < length)
        {
            throw "capacity is max";
        }

        if (m_size + length > m_capacity)
        {
            reserve(m_size + length);
        }

        memcpy(m_array + m_size, datas, length * sizeof(T));
        m_size += length;
    }

    template <typename STL>
    inline void append_data(const STL &stl)
    {
        size_t max_capacity = (std::numeric_limits<U>::max)() - m_size;

        if (max_capacity < stl.size())
        {
            throw "capacity is max";
        }

        if (m_size + static_cast<U>(stl.size()) > m_capacity)
        {
            reserve(m_size + static_cast<U>(stl.size()));
        }

        for (const auto &v : stl)
        {
            push_back(v);
        }
    }

    inline void append_data(const std::vector<T>& stl)
    {
        size_t max_capacity = (std::numeric_limits<U>::max)() - m_size;

        if (max_capacity < stl.size())
        {
            throw "capacity is max";
        }

        if (m_size + static_cast<U>(stl.size()) > m_capacity)
        {
            reserve(m_size + static_cast<U>(stl.size()));
        }

        memcpy(m_array + m_size, stl.data(), stl.size() * sizeof(T));
        m_size += static_cast<U>(stl.size());
    }

    inline void append_data(const std::string& str)
    {
        size_t max_capacity = (std::numeric_limits<U>::max)() - m_size;

        if (max_capacity < str.size())
        {
            throw "capacity is max";
        }

        if (m_size + static_cast<U>(str.size()) > m_capacity)
        {
            reserve(m_size + static_cast<U>(str.size()));
        }

        memcpy(m_array + m_size, str.c_str(), str.size());
        m_size += static_cast<U>(str.size());
    }

    std::string marshal_json(void) const;

    bool unmarshal_json(const std::string& json);

private:
    U   m_size;
    U   m_capacity;
    T*  m_array;
    T   m_cache[N];
    static const char* m_name;
};

template <typename T, typename U, size_t N>
const char* DataArray<T, U, N, true, true>::m_name = typeid(T).name();

template <typename T, typename U, size_t N>
class DataArray<T, U, N, false, true>
{
public:
    DataArray(void)
    {
        m_size = 0;
        m_capacity = static_cast<U>(N);
        m_array = m_cache;
    }

    ~DataArray()
    {
        for (U i = 0; i < m_size; i++)
        {
            (m_array + i)->~T();
        }

        if (m_array != m_cache)
        {
            S_FREE(m_array);
        }

        m_array = 0;
        m_size = 0;
        m_capacity = 0;
    }

    DataArray(const DataArray& src)
    {
        if (src.m_size <= static_cast<U>(N))
        {
            m_capacity = static_cast<U>(N);
            m_size = src.m_size;
            m_array = m_cache;
        }
        else
        {
            m_capacity = src.m_capacity;
            m_size = src.m_size;
            m_array = (T*)S_MALLOC_EX(sizeof(T) * m_capacity, m_name);
        }
        
        for (U i = 0; i < m_size; i++)
        {
            new(m_array + i)T(src.m_array[i]);
        }
    }

    DataArray& operator= (const DataArray& src)
    {
        for (U i = 0; i < m_size; i++)
        {
            (m_array + i)->~T();
        }
        m_size = 0;

        if (m_capacity < src.m_size)
        {
            m_capacity = src.m_capacity;

            if (m_array != m_cache)
            {
                S_FREE(m_array);
            }

            m_array = (T*)S_MALLOC_EX(sizeof(T)*m_capacity, m_name);
        }

        m_size = src.m_size;

        for (U i = 0; i < m_size; i++)
        {
            new(m_array + i)T(src.m_array[i]);
        }

        return *this;
    }

    bool operator != (const DataArray& src) const
    {
        return !(*this == src);
    }

    bool operator== (const DataArray& src) const
    {
        if (src.m_size != m_size)
        {
            return false;
        }

        for (U i = 0; i < m_size; i++)
        {
            if (m_array[i] != src.m_array[i])
            {
                return false;
            }
        }

        return true;
    }

    T& operator[](U idx) const
    {
        if (idx < m_size)
        {
            return m_array[idx];
        }
        else
        {
            //CRUSH_CODE();
            //return m_array[0];
            throw "operator [] overflow";
        }
    }

    void resize(U new_size)
    {
        if (new_size < m_size)
        {
            while (m_size > new_size)
            {
                (m_array + m_size - 1)->~T();
                m_size--;
            }
        }
        else if (new_size <= m_capacity)
        {
            while (m_size < new_size)
            {
                new(m_array + m_size)T();
                m_size++;
            }
        }
        else
        {
            m_capacity = new_size;

            T* new_array = (T*)S_MALLOC_EX(sizeof(T)*m_capacity, m_name);
            for (U i = 0; i < m_size; i++)
            {
                new(new_array + i)T(m_array[i]);
                (m_array + i)->~T();
            }

            if (m_array != m_cache)
            {
                S_FREE(m_array);
            }
            
            m_array = new_array;

            for (U i = m_size; i < m_capacity; i++)
            {
                new(m_array + i)T();
            }
            m_size = m_capacity;
        }
    }

    void reserve(U new_size)
    {
        if (new_size > m_capacity)
        {
            m_capacity = new_size;

            T* new_array = (T*)S_MALLOC_EX(sizeof(T)*m_capacity, m_name);
            for (U i = 0; i < m_size; i++)
            {
                new(new_array + i)T(m_array[i]);
                (m_array + i)->~T();
            }

            if (m_array != m_cache)
            {
                S_FREE(m_array);
            }
            
            m_array = new_array;
        }
    }

    void push_back(const T& val)
    {
        if (m_size < m_capacity)
        {
            new(m_array + m_size)T(val);
            m_size++;
        }
        else
        {
            if ((std::numeric_limits<U>::max)() - m_capacity < m_capacity)
            {
                if (m_capacity == (std::numeric_limits<U>::max)())
                {
                    //CRUSH_CODE();
                    throw "capacity is max";
                }
                else
                {
                    m_capacity = (std::numeric_limits<U>::max)();
                }
            }
            else
            {
                m_capacity += m_capacity;
            }

            T* new_array = (T*)S_MALLOC_EX(sizeof(T)*m_capacity, m_name);
            for (U i = 0; i < m_size; i++)
            {
                new(new_array + i)T(m_array[i]);
                (m_array + i)->~T();
            }
            new(new_array + m_size)T(val);
            ++m_size;

            if (m_array != m_cache)
            {
                S_FREE(m_array);
            }

            m_array = new_array;
        }
    }

    inline void erase(U idx)
    {
        if (idx < m_size)
        {
            (m_array + idx)->~T();

            for (U i = idx; i < m_size-1; i++)
            {
                new(m_array + i)T(m_array[i + 1]);
                (m_array + i + 1)->~T();
            }
            --m_size;
        }
        else
        {
            throw "operator erase overflow";
        }
    }

    void clear()
    {
        if (m_array)
        {
            for (U i = 0; i < m_size; i++)
            {
                (m_array + i)->~T();
            }
            m_size = 0;
        }
    }

    inline U size(void) const
    {
        return m_size;
    }

	inline size_t size_of_data(void) const
	{
		return sizeof(T);
	}

    inline U capacity(void) const
    {
        return m_capacity;
    }

	inline T* data(void) const
	{
		return m_array;
	}

    template <U S>
    inline void append_data(T(&datas)[S])
    {
        if ((std::numeric_limits<U>::max)() - m_size < S)
        {
            throw "capacity is max";
        }

        if (m_size + S > m_capacity)
        {
            reserve(m_size + S);
        }

        for (U i = 0; i < S; ++i)
        {
            push_back(datas[i]);
        }
    }

    inline void append_data(const T *datas, U length)
    {
        if ((std::numeric_limits<U>::max)() - m_size < length)
        {
            throw "capacity is max";
        }

        if (m_size + length > m_capacity)
        {
            reserve(m_size + length);
        }

        for (size_t i = 0; i < length; ++i)
        {
            push_back(datas[i]);
        }
    }

    template <typename STL>
    inline void append_data(const STL &stl)
    {
        if ((std::numeric_limits<U>::max)() - m_size < stl.size())
        {
            throw "capacity is max";
        }

        if (m_size + static_cast<U>(stl.size()) > m_capacity)
        {
            reserve(m_size + static_cast<U>(stl.size()));
        }

        for (const auto &v : stl)
        {
            push_back(v);
        }
    }

    std::string marshal_json(void) const;

    bool unmarshal_json(const std::string& json);

protected:
private:
    U   m_size;
    U   m_capacity;
    T*  m_array;
    T   m_cache[N];
    static const char* m_name;
};

template <typename T, typename U, size_t N>
const char* DataArray<T, U, N, false, true>::m_name = typeid(T).name();


class NetEnCode
{
public:
	NetEnCode(size_t reserve_size)
	{
		m_buffer = (char*)S_MALLOC(reserve_size);
		m_cur_pos = m_buffer;
		m_end = m_buffer + reserve_size;
	}
	~NetEnCode()
	{
		if (m_buffer)
		{
			S_FREE(m_buffer);
		}
		m_buffer = 0;
		m_end = 0;
		m_cur_pos = 0;
	}

	template<typename T>
	typename std::enable_if<std::is_integral<T>::value, void>::type AddIntegral(T var)
	{
		if (m_cur_pos + sizeof(T) > m_end)
		{
			__extend(sizeof(T));
		}

		*(T*)m_cur_pos = var;
		m_cur_pos += sizeof(T);
	}



	template<typename T, typename U>
	typename std::enable_if<std::is_integral<T>::value, void>::type AddArray(DataArray<T, U>& array)
	{
		AddIntegral(array.size());

		AddBlob(array.data(), array.size()*array.size_of_data());
	}

	template<typename T, typename U>
	typename std::enable_if<!std::is_integral<T>::value, void>::type AddArray(DataArray<T, U>& array)
	{
		AddIntegral(array.size());

		for (U i = 0; i < array.size(); i++)
		{
			array[i].EnCode(*this);
		}
	}

	void AddString(const char* str, size_t max_str_size)
	{
		size_t str_len = strnlen(str, max_str_size - 1);

		if (m_cur_pos + str_len + sizeof(unsigned short) > m_end)
		{
			__extend(str_len + sizeof(unsigned short));
		}

		AddIntegral((unsigned short)str_len);

		memcpy(m_cur_pos, str, str_len);
		m_cur_pos += str_len;
	}

	void AddBlob(const void* data, size_t data_size)
	{
		if (m_cur_pos + data_size > m_end)
		{
			__extend(data_size);
		}

		memcpy(m_cur_pos, data, data_size);

		m_cur_pos += data_size;
	}

    void* Reserve(size_t reserve_size)
    {
        void* cur_pos = m_cur_pos;

        if (m_cur_pos + reserve_size > m_end)
        {
            __extend(reserve_size);
        }

        m_cur_pos += reserve_size;

        return cur_pos;
    }

	inline void* Data()
	{
		return m_buffer;
	}

	inline size_t Length()
	{
		return m_cur_pos - m_buffer;
	}

	inline void Clear()
	{
		m_cur_pos = m_buffer;
	}
protected:
	void __extend(size_t min_size)
	{
		size_t cur_size = m_end - m_buffer;
		size_t cur_pos = m_cur_pos - m_buffer;

		if ((std::numeric_limits<size_t>::max)() - cur_size < cur_size / 2)
		{
			if (cur_size == (std::numeric_limits<size_t>::max)())
			{
                //CRUSH_CODE();
                throw "capacity is max";
			}
			else
			{
				cur_size = (std::numeric_limits<size_t>::max)();
			}
		}
		else
		{
			for (;;)
			{
				cur_size += cur_size / 2;
				if (cur_size - (m_cur_pos - m_buffer) >= min_size)
				{
					break;
				}

				if ((std::numeric_limits<size_t>::max)() - cur_size < cur_size / 2)
				{
                    //CRUSH_CODE();
                    throw "capacity is max";
				}
			}
		}


		m_buffer = (char*)S_REALLOC(m_buffer, cur_size);
		m_cur_pos = m_buffer + cur_pos;
		m_end = m_buffer + cur_size;
	}
private:
	char*		m_buffer;
	char*		m_cur_pos;
	char*		m_end;
};

class NetDeCode
{
public:
	NetDeCode(const void* data, size_t data_len)
	{
		m_buffer = (const char*)data;
		m_cur_pos = m_buffer;
		m_end = m_buffer + data_len;
	}
	~NetDeCode()
	{
		m_buffer = 0;
		m_cur_pos = 0;
		m_end = 0;
	}

	template<typename T>
	typename std::enable_if<std::is_integral<T>::value, const char*>::type DelIntegral(T& var)
	{
		if (m_cur_pos + sizeof(T) > m_end)
			return 0;

		var = *(T*)m_cur_pos;
		m_cur_pos += sizeof(T);

		return m_cur_pos;
	}

	template<typename T, typename U>
	typename std::enable_if<std::is_integral<T>::value, const char*>::type DelArray(DataArray<T, U>& array)
	{
		U array_size = 0;
		if (!DelIntegral(array_size))
			return 0;

		array.resize(array_size);

		if (!DelBlob(array.data(), array.size()*array.size_of_data()))
			return 0;

		return m_cur_pos;
	}

	template<typename T, typename U>
	typename std::enable_if<!std::is_integral<T>::value, const char*>::type DelArray(DataArray<T, U>& array)
	{
		U array_size = 0;
		if (!DelIntegral(array_size))
			return 0;

		array.resize(array_size);

		for (U i = 0; i < array_size; i++)
		{
			if (!array[i].DeCode(*this))
			{
				return 0;
			}
		}

		return m_cur_pos;
	}

	const char* DelString(char* str, size_t max_str_size)
	{
		unsigned short str_len;

		if (!DelIntegral(str_len))
			return 0;

		if (m_cur_pos + str_len > m_end)
			return 0;

		if (str_len + 1U > max_str_size)
			return 0;

		memcpy(str, m_cur_pos, str_len);
		str[str_len] = '\0';
		m_cur_pos += str_len;

		return m_cur_pos;
	}

	const char* DelBlob(void* data, size_t data_size)
	{
		if (m_cur_pos + data_size > m_end)
			return 0;

		memcpy(data, m_cur_pos, data_size);
		m_cur_pos += data_size;

		return m_cur_pos;
	}

    const void* Discard(size_t discard_size)
    {
        const void* cur_pos = m_cur_pos;

        if (m_cur_pos + discard_size > m_end)
            return 0;

        m_cur_pos += discard_size;

        return cur_pos;
    }

	inline void Reset(size_t pos = 0)
	{
		if (m_buffer + pos  < m_end)
		{
			m_cur_pos = m_buffer + pos;
		}
	}

	inline const char* DataByPos(size_t pos)
	{
		if (m_buffer + pos < m_end)
		{
			return m_buffer + pos;
		}

		return 0;
	}

	inline size_t CurPos()
	{
		return m_cur_pos - m_buffer;
	}

protected:
private:
	const char*	m_buffer;
	const char* m_cur_pos;
	const char* m_end;
};

class JsonAllocator {
public:
    static const bool kNeedFree = true;
    void* Malloc(size_t size) {
        if (size) //  behavior of malloc(0) is implementation defined.
            return S_MALLOC_EX(size, u8"JsonAllocator");
        else
            return NULL; // standardize to returning NULL.
    }
    void* Realloc(void* originalPtr, size_t originalSize, size_t newSize) {
        (void)originalSize;
        if (newSize == 0) {
            S_FREE(originalPtr);
            return NULL;
        }
        return S_REALLOC_EX(originalPtr, newSize, u8"JsonAllocator");
    }
    static void Free(void* ptr) { S_FREE(ptr); }
};

class JsonEnCode
{
public:
    JsonEnCode(size_t capacity = 1024)
        :m_buffer(&m_allocator, capacity), m_writer(m_buffer)
    {

    }
    ~JsonEnCode()
    {

    }

    template<typename T, size_t N>
    typename std::enable_if<
        !std::is_integral<T>::value, void>::type AddIntegral(const char(&key)[N], T var)
    {

    }

    template<size_t N>
    void AddIntegral(const char(&key)[N], char var)
    {
        m_writer.Key(key, N - 1);
        m_writer.Int(var);
    }

    template<size_t N>
    void AddIntegral(const char(&key)[N], unsigned char var)
    {
        m_writer.Key(key, N - 1);
        m_writer.Uint(var);
    }

    template<size_t N>
    void AddIntegral(const char(&key)[N], short var)
    {
        m_writer.Key(key, N - 1);
        m_writer.Int(var);
    }

    template<size_t N>
    void AddIntegral(const char(&key)[N], unsigned short var)
    {
        m_writer.Key(key, N - 1);
        m_writer.Uint(var);
    }

    template<size_t N>
    void AddIntegral(const char(&key)[N], int var)
    {
        m_writer.Key(key, N - 1);
        m_writer.Int(var);
    }

    template<size_t N>
    void AddIntegral(const char(&key)[N], unsigned int var)
    {
        m_writer.Key(key, N - 1);
        m_writer.Uint(var);
    }

    template<size_t N>
    void AddIntegral(const char(&key)[N], long long var)
    {
        m_writer.Key(key, N - 1);
        m_writer.Int64(var);
    }

    template<size_t N>
    void AddIntegral(const char(&key)[N], unsigned long long var)
    {
        m_writer.Key(key, N - 1);
        m_writer.Uint64(var);
    }

    template<typename T, typename U>
    typename std::enable_if<!std::is_integral<T>::value, void>::type AddArray(const DataArray<T, U>& array)
    {
        m_writer.StartArray();
        for (U i = 0; i < array.size(); i++)
        {
            array[i].ToJson(*this);
        }
        m_writer.EndArray();
    }

    template<typename U>
    void AddArray(const DataArray<unsigned char, U>& array)
    {
        m_writer.String(reinterpret_cast<const char*>(array.data()),
            static_cast<rapidjson::SizeType>(array.size() * array.size_of_data()), true);
    }

    template<typename U>
    void AddArray(const DataArray<char, U>& array)
    {
        m_writer.String(reinterpret_cast<const char*>(array.data()),
            static_cast<rapidjson::SizeType>(array.size() * array.size_of_data()), true);
    }

    template<typename U>
    void AddArray(const DataArray<unsigned short, U>& array)
    {
        m_writer.StartArray();

        for (U i = 0; i < array.size(); i++)
        {
            m_writer.Uint(array[i]);
        }

        m_writer.EndArray();
    }

    template<typename U>
    void AddArray(const DataArray<short, U>& array)
    {
        m_writer.StartArray();

        for (U i = 0; i < array.size(); i++)
        {
            m_writer.Int(array[i]);
        }

        m_writer.EndArray();
    }

    template<typename U>
    void AddArray(const DataArray<unsigned int, U>& array)
    {
        m_writer.StartArray();

        for (U i = 0; i < array.size(); i++)
        {
            m_writer.Uint(array[i]);
        }

        m_writer.EndArray();
    }

    template<typename U>
    void AddArray(const DataArray<int, U>& array)
    {
        m_writer.StartArray();

        for (U i = 0; i < array.size(); i++)
        {
            m_writer.Int(array[i]);
        }

        m_writer.EndArray();
    }

    template<typename U>
    void AddArray(const DataArray<unsigned long long, U>& array)
    {
        m_writer.StartArray();

        for (U i = 0; i < array.size(); i++)
        {
            m_writer.Uint64(array[i]);
        }

        m_writer.EndArray();
    }

    template<typename U>
    void AddArray(const DataArray<long long, U>& array)
    {
        m_writer.StartArray();

        for (U i = 0; i < array.size(); i++)
        {
            m_writer.Int64(array[i]);
        }

        m_writer.EndArray();
    }

    template<typename T, size_t M>
    typename std::enable_if<!std::is_integral<T>::value, void>::type AddArrayOld(T(&array)[M], size_t count)
    {
        m_writer.StartArray();

        size_t max_count = std::min(M, count);

        for (size_t i = 0; i < max_count; i++)
        {
            array[i].ToJson(*this);
        }
        m_writer.EndArray();
    }

    template<size_t M>
    void AddArrayOld(const char(&array)[M], size_t count)
    {
        m_writer.StartArray();

        size_t max_count = std::min(M, count);
        m_writer.String(reinterpret_cast<const char*>(array),
            static_cast<rapidjson::SizeType>(max_count), true);
        m_writer.EndArray();
    }

    template<size_t M>
    void AddArrayOld(const unsigned char(&array)[M], size_t count)
    {
        m_writer.StartArray();

        size_t max_count = std::min(M, count);
        m_writer.String(reinterpret_cast<const char*>(array),
            static_cast<rapidjson::SizeType>(max_count), true);
        m_writer.EndArray();
    }

    template<size_t M>
    void AddArrayOld(const short(&array)[M], size_t count)
    {
        m_writer.StartArray();

        size_t max_count = std::min(M, count);

        for (size_t i = 0; i < max_count; i++)
        {
            m_writer.Int(array[i]);
        }

        m_writer.EndArray();
    }

    template<size_t M>
    void AddArrayOld(const unsigned short(&array)[M], size_t count)
    {
        m_writer.StartArray();

        size_t max_count = std::min(M, count);

        for (size_t i = 0; i < max_count; i++)
        {
            m_writer.Uint(array[i]);
        }

        m_writer.EndArray();
    }

    template<size_t M>
    void AddArrayOld(const int(&array)[M], size_t count)
    {
        m_writer.StartArray();

        size_t max_count = std::min(M, count);

        for (size_t i = 0; i < max_count; i++)
        {
            m_writer.Int(array[i]);
        }

        m_writer.EndArray();
    }

    template<size_t M>
    void AddArrayOld(const unsigned int(&array)[M], size_t count)
    {
        m_writer.StartArray();

        size_t max_count = std::min(M, count);

        for (size_t i = 0; i < max_count; i++)
        {
            m_writer.Uint(array[i]);
        }

        m_writer.EndArray();
    }

    template<size_t M>
    void AddArrayOld(const long long(&array)[M], size_t count)
    {
        m_writer.StartArray();

        size_t max_count = std::min(M, count);

        for (size_t i = 0; i < max_count; i++)
        {
            m_writer.Int64(array[i]);
        }

        m_writer.EndArray();
    }

    template<size_t M>
    void AddArrayOld(const unsigned long long(&array)[M], size_t count)
    {
        m_writer.StartArray();

        size_t max_count = std::min(M, count);

        for (size_t i = 0; i < max_count; i++)
        {
            m_writer.Uint64(array[i]);
        }

        m_writer.EndArray();
    }

    template<size_t N>
    void AddString(const char(&key)[N], const char* str, size_t max_str_size)
    {
        m_writer.Key(key, N - 1);

        m_writer.String(str, static_cast<rapidjson::SizeType>(strnlen(str, max_str_size - 1)), true);
    }

    std::string ToString(void)
    {
        return std::string(m_buffer.GetString(), m_buffer.GetLength());
    }

    inline rapidjson::Writer<rapidjson::GenericStringBuffer<rapidjson::UTF8<>, JsonAllocator>>& Writer() { return m_writer; }

protected:
private:
    JsonAllocator m_allocator;
    rapidjson::GenericStringBuffer<rapidjson::UTF8<>, JsonAllocator> m_buffer;
    rapidjson::Writer<rapidjson::GenericStringBuffer<rapidjson::UTF8<>, JsonAllocator>> m_writer;
};

class JsonHandler
{
public:
    JsonHandler(JsonHandler* parent) :m_parent_handler(parent) {}
    virtual ~JsonHandler() {}
    virtual bool Null() { return false; }
    virtual bool Bool(bool b) { (void)b; return false; }
    virtual bool Int(int i) { (void)i; return false; }
    virtual bool Uint(unsigned u) { (void)u; return false; }
    virtual bool Int64(int64_t i) { (void)i; return false; }
    virtual bool Uint64(uint64_t u) { (void)u; return false; }
    virtual bool Double(double d) { (void)d; return false; }
    virtual bool RawNumber(const char* str, rapidjson::SizeType length, bool copy)
    {
        (void)str;
        (void)length;
        (void)copy;

        return false;
    }

    virtual bool String(const char* str, rapidjson::SizeType length, bool copy)
    {
        (void)str;
        (void)length;
        (void)copy;

        return false;
    }

    virtual bool Key(const char* str, rapidjson::SizeType length, bool copy)
    {
        (void)str;
        (void)length;
        (void)copy;

        return false;
    }

    virtual JsonHandler* StartObject() { return nullptr; }
    virtual bool EndObject(JsonHandler* childen, rapidjson::SizeType memberCount) { (void)childen; (void)memberCount; return false; }
    virtual JsonHandler* StartArray() { return nullptr; }
    virtual bool EndArray(JsonHandler* childen, rapidjson::SizeType elementCount) { (void)childen; (void)elementCount; return false; }
    inline JsonHandler* ParentHandler(void) { return m_parent_handler; }
    inline void SetParentHandler(JsonHandler* parent_handler) { m_parent_handler = parent_handler; }
protected:
private:
    JsonHandler* m_parent_handler;
};

class HoleJsonHandler :
    public JsonHandler
{
public:
    HoleJsonHandler(JsonHandler* parent = nullptr)
        :JsonHandler(parent), m_real_parent_handler(parent), m_ref_count(0){}
    ~HoleJsonHandler(){}

    void Reset(JsonHandler* parent = nullptr)
    {
        m_real_parent_handler = parent;
        m_ref_count = 0;
        SetParentHandler(m_real_parent_handler);
    }

    bool Null() override { return true; }
    bool Bool(bool b) override { (void)b; return true; }
    bool Int(int i) override { (void)i; return true; }
    bool Uint(unsigned u) override { (void)u; return true; }
    bool Int64(int64_t i) override { (void)i; return true; }
    bool Uint64(uint64_t u) override { (void)u; return true; }
    bool Double(double d) override { (void)d; return true; }
    bool RawNumber(const char* str, rapidjson::SizeType length, bool copy) override
    {
        (void)str;
        (void)length;
        (void)copy;

        return true;
    }

    bool String(const char* str, rapidjson::SizeType length, bool copy) override
    {
        (void)str;
        (void)length;
        (void)copy;

        return true;
    }

    virtual bool Key(const char* str, rapidjson::SizeType length, bool copy) override
    {
        (void)str;
        (void)length;
        (void)copy;

        return true;
    }

   JsonHandler* StartObject() override 
   {
       if (!m_ref_count)
       {
           SetParentHandler(this);
       }
       ++m_ref_count;
       return this;
   }

   bool EndObject(JsonHandler* childen, rapidjson::SizeType memberCount) override 
   {
       (void)childen;
       (void)memberCount;

       --m_ref_count;

       if (!m_ref_count)
       {
           SetParentHandler(m_real_parent_handler);
       }
       return true;
   }

   JsonHandler* StartArray() override 
   {
       if (!m_ref_count)
       {
           SetParentHandler(this);
       }
       ++m_ref_count;
       return this;
   }

   bool EndArray(JsonHandler* childen, rapidjson::SizeType elementCount) override 
   { 
       (void)childen;
       (void)elementCount;

       --m_ref_count;

       if (!m_ref_count)
       {
           SetParentHandler(m_real_parent_handler);
       }
       return true;
   }

   bool IsAllMemberSet()
   { 
       return true; 
   }

   JsonHandler* m_real_parent_handler;
   size_t m_ref_count;
};

class JsonDeCode
{
public:
    JsonDeCode(JsonHandler* handler = nullptr)
        :m_handler(nullptr), m_root_handler(handler) {}
    ~JsonDeCode() {}
    bool Null() { return m_handler->Null(); }
    bool Bool(bool b) { return m_handler->Bool(b); }
    bool Int(int i) { return m_handler->Int(i); }
    bool Uint(unsigned u) { return m_handler->Uint(u); }
    bool Int64(int64_t i) { return m_handler->Int64(i); }
    bool Uint64(uint64_t u) { return m_handler->Uint64(u); }
    bool Double(double d) { return m_handler->Double(d); }
    bool RawNumber(const char* str, rapidjson::SizeType length, bool copy)
    {
        return m_handler->RawNumber(str, length, copy);
    }
    bool String(const char* str, rapidjson::SizeType length, bool copy)
    {
        return m_handler->String(str, length, copy);
    }

    bool Key(const char* str, rapidjson::SizeType length, bool copy)
    {
        return m_handler->Key(str, length, copy);
    }

    bool StartObject()
    {
        if (m_handler)
        {
            m_handler = m_handler->StartObject();
        }
        else
        {
            m_handler = m_root_handler;
        }

        return m_handler != nullptr;
    }

    bool EndObject(rapidjson::SizeType memberCount)
    {
        JsonHandler* childen = m_handler;
        m_handler = m_handler->ParentHandler();

        if (m_handler)
        {
            return m_handler->EndObject(childen, memberCount);
        }
        else
        {
            if (childen == m_root_handler)
            {
                return true;
            }
            return false;
        }
    }

    bool StartArray()
    {
        if (m_handler)
        {
            m_handler = m_handler->StartArray();
        }
        else
        {
            m_handler = m_root_handler;
        }

        return m_handler != nullptr;
    }

    bool EndArray(rapidjson::SizeType elementCount)
    {
        JsonHandler* childen = m_handler;
        m_handler = m_handler->ParentHandler();

        if (m_handler)
        {
            return m_handler->EndArray(childen, elementCount);
        }
        else
        {
            if (childen == m_root_handler)
            {
                return true;
            }
            return false;
        }
    }

protected:
private:
    JsonHandler* m_handler;
    JsonHandler* m_root_handler;
};

template<size_t N>
inline void JsonDelString(char(&Destination)[N], const char* Source, rapidjson::SizeType length) throw()
{
    if (nullptr == Source) {
        Destination[0] = '\0';
        return;
    }

    size_t nSrcLen = (length < (N - 1)) ? length : (N - 1);
    memcpy(Destination, Source, nSrcLen + 1);
    Destination[N - 1] = '\0';
}

template<typename T>
inline void JsonDelIntegral(T& var, const char* str, rapidjson::SizeType length)
{
    (void)length;

    typename std::enable_if<!std::is_integral<T>::value, T>::type& data = var;
    data = static_cast<T>(strtol(str, 0, 10));
}

template<>
inline void JsonDelIntegral(char& var, const char* str, rapidjson::SizeType length)
{
    (void)length;
    var = static_cast<char>(strtol(str, 0, 10));
}

template<>
inline void JsonDelIntegral(unsigned char& var, const char* str, rapidjson::SizeType length)
{
    (void)length;
    var = static_cast<char>(strtoul(str, 0, 10));
}

template<>
inline void JsonDelIntegral(short& var, const char* str, rapidjson::SizeType length)
{
    (void)length;
    var = static_cast<short>(strtol(str, 0, 10));
}

template<>
inline void JsonDelIntegral(unsigned short& var, const char* str, rapidjson::SizeType length)
{
    (void)length;
    var = static_cast<unsigned short>(strtoul(str, 0, 10));
}

template<>
inline void JsonDelIntegral(int& var, const char* str, rapidjson::SizeType length)
{
    (void)length;
    var = static_cast<int>(strtol(str, 0, 10));
}

template<>
inline void JsonDelIntegral(unsigned int& var, const char* str, rapidjson::SizeType length)
{
    (void)length;
    var = static_cast<unsigned int>(strtoul(str, 0, 10));
}

template<>
inline void JsonDelIntegral(long long& var, const char* str, rapidjson::SizeType length)
{
    (void)length;
    var = static_cast<long long>(strtoll(str, 0, 10));
}

template<>
inline void JsonDelIntegral(unsigned long long& var, const char* str, rapidjson::SizeType length)
{
    (void)length;
    var = static_cast<unsigned long long>(strtoull(str, 0, 10));
}
/// <summary>
/// DataArrayOldHandler
/// </summary>

class IntegralDataArrayOldHandler :
    public JsonHandler
{
};

template < typename T, size_t N, typename H = IntegralDataArrayOldHandler, bool is_integral = std::is_integral<T>::value >
class DataArrayOldHandler :
    public JsonHandler
{

};

template <typename T, size_t N, typename H>
class DataArrayOldHandler<T, N, H, false> :
    public JsonHandler
{
public:
    DataArrayOldHandler(T(&data)[N], JsonHandler* parent, size_t max_count)
        :JsonHandler(parent), m_data(data), m_handler(nullptr), m_max_count(max_count), 
        m_cur_count(0), m_element_count(0)
    {
        m_max_count = std::min(N, max_count);
    }
    ~DataArrayOldHandler()
    {
        if (m_handler)
        {
            S_DELETE(m_handler);
            m_handler = nullptr;
        }
    }

    JsonHandler* StartObject() override
    {
        if (m_handler)
        {
            return nullptr;
        }

        if (m_cur_count < m_max_count)
        {
            m_handler = S_NEW(H, 1, m_data[m_cur_count], this);
            ++m_cur_count;

            return m_handler;
        }
        else
        {
            return nullptr;
        }
    }

    bool EndObject(JsonHandler* childen, rapidjson::SizeType memberCount) override
    {
        (void)memberCount;
        if (m_handler == childen)
        {
            if (m_handler->IsAllMemberSet())
            {
                ++m_element_count;
            }
            S_DELETE(m_handler);
            m_handler = nullptr;

            return true;
        }
        else
        {
            return false;
        }
    }

    bool IsAllMemberSet(void)
    {
        return m_element_count == m_cur_count;
    }

    void ResetState(void)
    {
        m_element_count = 0;
    }
protected:
private:
    T(&m_data)[N];
    H* m_handler;
    size_t m_max_count;
    size_t m_cur_count;
    size_t m_element_count;
};

template < typename T, size_t N >
class DataArrayOldHandler<T, N, IntegralDataArrayOldHandler, true> :
    public JsonHandler
{
public:
    DataArrayOldHandler(T(&data)[N], JsonHandler* parent, size_t max_count)
        :JsonHandler(parent), m_data(data), m_max_count(max_count), m_cur_count(0)
    {
        m_max_count = std::min(N, max_count);
    }
    ~DataArrayOldHandler(){}

    bool String(const char* str, rapidjson::SizeType length, bool copy) override
    {
        (void)copy;
        memcpy(m_data, str, std::min(N, static_cast<size_t>(length)));

        return true;
    }

    bool RawNumber(const char* str, rapidjson::SizeType length, bool copy) override
    {
        (void)copy;

        if (m_cur_count < m_max_count)
        {
            JsonDelIntegral(m_data[m_cur_count], str, length);
            ++m_cur_count;
        }

        return true;
    }

    bool IsAllMemberSet(void)
    {
        return true;
    }

private:
    T(&m_data)[N];
    size_t m_max_count;
    size_t m_cur_count;
};

/// <summary>
/// DataArrayHandler
/// </summary>

class IntegralDataArrayHandler :
    public JsonHandler
{

};

template < typename T, typename U, typename H = IntegralDataArrayHandler, size_t N = 254 / sizeof(T) + 1, bool is_integral = std::is_integral<T>::value >
class DataArrayHandler :
    public JsonHandler
{

};

template <typename T, typename U, typename H, size_t N>
class DataArrayHandler<T, U, H, N, false> :
    public JsonHandler
{
public:
    DataArrayHandler(DataArray<T, U, N>& data, JsonHandler* parent)
        :JsonHandler(parent), m_data(data), m_handler(m_t, this), m_element_count(0) {}
    ~DataArrayHandler()
    {
    }

    JsonHandler* StartObject() override
    {
        m_t.Reset();
        m_handler.ResetState();

        return &m_handler;
    }

    bool EndObject(JsonHandler* childen, rapidjson::SizeType memberCount) override
    {
        (void)memberCount;

        m_data.push_back(m_t);

        if (m_handler.IsAllMemberSet())
        {
            m_element_count++;
        }

        if (&m_handler == childen)
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    bool IsAllMemberSet(void)
    {
        return m_element_count == m_data.size();
    }

    void ResetState(void)
    {
        m_element_count = 0;
    }

private:
    DataArray<T, U, N>& m_data;
    T                   m_t;
    H                   m_handler;
    U                   m_element_count;
};

template < typename T, typename U, size_t N >
class DataArrayHandler<T, U, IntegralDataArrayHandler, N, true> :
    public JsonHandler
{
public:
    DataArrayHandler(DataArray<T, U, N>& data, JsonHandler* parent)
        :JsonHandler(parent), m_data(data) {}
    ~DataArrayHandler() {}

    bool String(const char* str, rapidjson::SizeType length, bool copy) override
    {
        (void)copy;

        m_data.append_data(reinterpret_cast<const T*>(str), static_cast<U>(length));

        return true;
    }

    bool RawNumber(const char* str, rapidjson::SizeType length, bool copy) override
    {
        (void)copy;

        T var = 0;
        JsonDelIntegral(var, str, length);

        m_data.push_back(var);

        return true;
    }

    bool IsAllMemberSet(void)
    {
        return true;
    }

private:
    DataArray<T, U, N>& m_data;
};

template <typename T, typename U, size_t N>
std::string DataArray<T, U, N, true, true>::marshal_json(void) const
{
    JsonEnCode json_encode(4096);

    json_encode.AddArray(*this);

    return json_encode.ToString();
}

template <typename T, typename U, size_t N>
bool DataArray<T, U, N, true, true>::unmarshal_json(const std::string& json)
{
    DataArrayHandler<T, U> h(*this, nullptr);
    JsonDeCode jd(&h);

    JsonAllocator json_allocator;
    rapidjson::GenericReader<rapidjson::UTF8<>, rapidjson::UTF8<>, JsonAllocator> rd(&json_allocator, 1024);
    rapidjson::StringStream ss(json.c_str());
    return rd.Parse<rapidjson::kParseNumbersAsStringsFlag>(ss, jd);
}

template <typename T, typename U, size_t N>
std::string DataArray<T, U, N, false, true>::marshal_json(void) const
{
    JsonEnCode json_encode(4096);

    json_encode.AddArray(*this);

    return json_encode.ToString();
}

template <typename T, typename U, size_t N>
bool DataArray<T, U, N, false, true>::unmarshal_json(const std::string& json)
{
    DataArrayHandler<T, U, typename T::Handler, N> h(*this, nullptr);
    JsonDeCode jd(&h);

    JsonAllocator json_allocator;
    rapidjson::GenericReader<rapidjson::UTF8<>, rapidjson::UTF8<>, JsonAllocator> rd(&json_allocator, 1024);
    rapidjson::StringStream ss(json.c_str());
    return rd.Parse<rapidjson::kParseNumbersAsStringsFlag>(ss, jd);
}

class protocol_base
{
public:
    const unsigned short module_id;
    const unsigned short protocol_id;

    protocol_base(unsigned short m_id,
        unsigned short p_id) :
        module_id(m_id), protocol_id(p_id) {}

    virtual ~protocol_base() {}
public:
    virtual bool EnCodeEx(NetEnCode& net_data) = 0;
    virtual bool DeCodeEx(NetDeCode& net_data) = 0;
protected:
private:
};

template<typename T>
class Protocol: public protocol_base
{
public:
    ~Protocol(){}

    bool EnCode(NetEnCode& net_data)
    {
        net_data.Clear();
        return false;
    }

    bool DeCode(NetDeCode& net_data)
    {
        net_data.Reset();
        return false;
    }

protected:
private:
    Protocol(unsigned short m_id, unsigned short p_id)
        :protocol_base(m_id, p_id) {}
    friend T;
};

#endif
