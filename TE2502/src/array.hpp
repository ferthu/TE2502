#pragma once

template<typename T, size_t N>
class Array
{
public:
	T& operator[](size_t n)
	{
#ifdef _DEBUG
		if (n >= N)
			__debugbreak();
#endif

		return arr[n];
	}

	const T& operator[](size_t n) const
	{
#ifdef _DEBUG
		if (n >= N)
			__debugbreak();
#endif

		return arr[n];
	}

private:
	T arr[N];
};