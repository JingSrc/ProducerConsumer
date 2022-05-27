#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>

template <typename T>
class ProducerConsumerIterator
{
public:
	using container_type         = T;
	using container_type_pointer = container_type*;
	using value_type             = typename container_type::value_type;
	using reference              = typename container_type::reference;
	using const_reference        = const reference;
	using pointer                = value_type*;
	using const_pointor          = const value_type*;
	using size_type              = typename container_type::size_type;
	using difference_type        = typename container_type::difference_type;
	using iterator_category      = std::forward_iterator_tag;

	ProducerConsumerIterator(container_type_pointer c, value_type v, bool end)
		: end_{end}
		, v_{v}
		, c_(c) {}

	ProducerConsumerIterator(const ProducerConsumerIterator& rhs)
	{
		*this = rhs;
	}

	ProducerConsumerIterator &operator=(const ProducerConsumerIterator& rhs) noexcept
	{
		end_ = rhs.end_;
		v_ = rhs.v_;
		c_ = rhs.c_;

		return *this;
	}

	bool operator==(const ProducerConsumerIterator& rhs) const noexcept
	{
		if (c_ == rhs.c_ && end_ == rhs.end_ && v_ == rhs.v_)
			return true;
		return false;
	}

	bool operator!=(const ProducerConsumerIterator& rhs) const noexcept
	{
		return !(*this == rhs);
	}

	ProducerConsumerIterator& operator++()
	{
		*this = c_->next();
		return *this;
	}

	ProducerConsumerIterator& operator++(int)
	{
		*this = c_->next();
		return *this;
	}

	reference operator*()
	{
		return v_;
	}

	pointer operator->()
	{
		return &v_;
	}

private:
	bool end_;
	value_type v_;
	container_type_pointer c_;
};

template<typename T>
class ProducerConsumer
{
public:
    using container_type  = std::queue<T>;
    using value_type      = typename container_type::value_type;
    using reference       = typename container_type::reference;
    using iterator        = ProducerConsumerIterator<ProducerConsumer>;
    using size_type       = typename container_type::size_type;
    using difference_type = ptrdiff_t;

    ProducerConsumer()
	    : end_{true}
    {
		
    }

    ~ProducerConsumer()
    {
		close();
    }

	ProducerConsumer(ProducerConsumer&) = delete;
	ProducerConsumer(ProducerConsumer&&) = delete;
	ProducerConsumer &operator=(ProducerConsumer&) = delete;
	ProducerConsumer& operator=(ProducerConsumer&&) = delete;

	void open() { end_ = false; }
	void close() { end_ = true; }

    iterator begin() noexcept
    {
		return next();
    }

    iterator end() noexcept
    {
		return ProducerConsumerIterator<ProducerConsumer>{this, value_type{}, true};
    }

    bool empty() const
    {
		std::unique_lock<std::mutex> lock{ value_mutex_ };
		return values_.empty();
    }

    size_type size() const
    {
		std::unique_lock<std::mutex> lock{ value_mutex_ };
		return values_.size();
    }

	iterator next() noexcept
    {
		auto v = pop();
		return ProducerConsumerIterator<ProducerConsumer>{this, v, end_};
    }

	value_type pop() noexcept
    {
		if (end_)
			return value_type{};

		std::unique_lock<std::mutex> lock{ mutex_ };
		cond_.wait(lock, [this] { return (this->end_ || this->values_.size() > 0); });

		value_mutex_.lock();
		if (end_ || values_.empty()) {
			value_mutex_.unlock();
			return value_type{};
		}

		auto v = values_.front();
		values_.pop();
		value_mutex_.unlock();
		return v;
    }

	void push(value_type v) noexcept
    {
		if (end_)
			return;

		{
			std::unique_lock<std::mutex> value_lock{ value_mutex_ };
			values_.push(v);
		}
		cond_.notify_one();
    }

private:
	bool end_;
    container_type values_;

	mutable std::mutex value_mutex_;
	mutable std::mutex mutex_;
    std::condition_variable cond_;
};
