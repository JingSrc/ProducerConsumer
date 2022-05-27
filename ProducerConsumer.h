#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>
#include <atomic>

template <typename T>
class ProducerConsumerIterator
{
public:
	using container_type         = T;
	using container_type_pointer = container_type*;
	using value_type             = typename container_type::value_type;
	using reference              = typename container_type::reference;
	using pointer                = value_type*;
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
		v_ = c_->pop();
		end_ = !c_->is_opened();
		return *this;
	}

	reference operator*() { return v_; }
	pointer operator->() { return &v_; }

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

	explicit ProducerConsumer(size_t size = 0)
		: end_{ true }, max_size_{ size } {}

	~ProducerConsumer() = default;

	ProducerConsumer(ProducerConsumer&) = delete;
	ProducerConsumer(ProducerConsumer&&) = delete;
	ProducerConsumer& operator=(ProducerConsumer&) = delete;
	ProducerConsumer& operator=(ProducerConsumer&&) = delete;

	bool is_opened() const { return !end_; }

	void open() { end_ = false; }
	void close()
	{
		end_ = true;
		not_full_.notify_all();
		not_empty_.notify_all();
	}

	iterator begin() noexcept
	{
		if (end_)
			return end();

		return ProducerConsumerIterator<ProducerConsumer>{this, pop(), end_};
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

	value_type pop() noexcept
	{
		if (end_)
			return value_type{};


		std::unique_lock<std::mutex> lock{ mutex_ };
		not_empty_.wait(lock, [this]{ return (this->end_ || this->values_.size() > 0); });

		value_mutex_.lock();
		if (end_ || values_.empty()) {
			value_mutex_.unlock();
			return value_type{};
		}

		auto v = values_.front();
		values_.pop();
		value_mutex_.unlock();
		not_full_.notify_one();
		return v;
	}

	void push(value_type v) noexcept
	{
		if (end_)
			return;

		value_mutex_.lock();
		if (max_size_ != 0 && values_.size() >= max_size_) {
			value_mutex_.unlock();

			std::unique_lock<std::mutex> lock{ mutex_ };
			not_full_.wait(lock, [this] { return (this->end_ || this->values_.size() < this->max_size_); });
			value_mutex_.lock();
		}

		values_.push(v);
		value_mutex_.unlock();
		not_empty_.notify_one();
	}

private:
	bool end_;
	size_t max_size_;
	container_type values_;

	mutable std::mutex value_mutex_;
	mutable std::mutex mutex_;
	std::condition_variable not_full_;
	std::condition_variable not_empty_;
};

template<typename T>
class ProducerConsumer_Ex
{
public:
	using inner_type    = ProducerConsumer<T>;
	using value_type    = typename inner_type::value_type;
	using size_type     = typename inner_type::size_type;
	using producer_type = std::function<value_type()>;
	using consumer_type = std::function<void(const T&)>;

	explicit ProducerConsumer_Ex(size_t size = 0)
		: running_{ false }, count_{ 0 }, inner_{ size } {}

	explicit ProducerConsumer_Ex(producer_type prod, size_t size = 0)
		: ProducerConsumer_Ex{ size }
	{
		open();
		produce(prod);
	}

	~ProducerConsumer_Ex() = default;

	ProducerConsumer_Ex(ProducerConsumer_Ex&) = delete;
	ProducerConsumer_Ex(ProducerConsumer_Ex&&) = delete;
	ProducerConsumer_Ex& operator=(ProducerConsumer_Ex&) = delete;
	ProducerConsumer_Ex& operator=(ProducerConsumer_Ex&&) = delete;

	bool is_opened() const { return inner_.is_opened(); }

	void open()
	{
		running_ = true;
		inner_.open();
	}
	void close()
	{
		running_ = false;
		inner_.close();

		while (count_ != 0)
			std::this_thread::yield();
	}

	void produce(producer_type prod)
	{
		++count_;
		std::thread thd([this, prod] {
			while (this->running_) {
				this->push(prod());
			}
			--this->count_;
		});
		thd.detach();
	}

	void consume(consumer_type consumer)
	{
		++count_;
		std::thread thd([this, consumer] {
			for (const auto &v : this->inner_) {
				consumer(v);
			}
			--this->count_;
		});
		thd.detach();
	}

	bool empty() const { return inner_.empty(); }
	size_type size() const { return inner_.size(); }

	void push(value_type v) noexcept { inner_.push(v); }

private:
	bool running_;
	std::atomic_int count_;
	inner_type inner_;
};
