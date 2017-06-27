#pragma once

#include <thread>
#include <chrono>

#include <unordered_map>
#include <unordered_set>
#include <typeindex>
#include <algorithm>
#include <deque>

using handle_t = std::size_t;

struct BaseShelf {
	virtual ~BaseShelf() {};

	virtual handle_t allocate() = 0;
	virtual void release(const handle_t& handle) = 0;
	virtual void allocate(std::size_t count, handle_t* handles) = 0;
	virtual void release(std::size_t count, handle_t* handles) = 0;
	virtual void clear() = 0;
	virtual std::size_t size() const = 0;
	virtual bool empty() const = 0;
	virtual bool freelist_empty() const = 0;
};

template <typename T>
struct Shelf : BaseShelf {
	using item_set_t = std::deque<T>;
	using handle_set_t = std::vector<handle_t>;
	static const handle_t handle_null = std::numeric_limits<handle_t>::max();

	// Data
	item_set_t item_set;
	handle_set_t sparse_set;
	handle_set_t reverse_set;
	handle_t freelist_front = handle_null;
	handle_t freelist_back = handle_null;

	// Access operators
	T& at(const handle_t& handle);
	const T& at(const handle_t& handle) const;
	T& operator[](const handle_t& handle) { return item_set[sparse_set[handle]]; }
	const T& operator[](const handle_t& handle) const { return item_set[sparse_set[handle]]; }

	// Mutators
	handle_t allocate();
	void release(const handle_t& handle);
	void allocate(std::size_t count, handle_t* handles);
	void release(std::size_t count, handle_t* handles);
	void clear() { item_set.clear(); reverse_set.clear(); }

	// Info
	std::size_t size() const { return item_set.size(); }
	bool empty() const { return item_set.empty(); }
	bool freelist_empty() const { return freelist_front == handle_null;  }
	
	// Iterators
	typename item_set_t::iterator begin() { return item_set.begin(); }
	typename item_set_t::iterator end() { return item_set.end(); }
	typename item_set_t::const_iterator cbegin() const { return item_set.cbegin(); }
	typename item_set_t::const_iterator cend() const { return item_set.cend(); }
};

struct Market {
	std::unordered_map<std::type_index, std::unique_ptr<BaseShelf>> shelves;

	void clear() {
		shelves.clear();
	}

	void release(std::type_index type, handle_t index) {
		if (!shelves.count(type))
			return;

		auto shelf = shelves[type].get();
		shelf->release(1, &index);
	}

	bool isvalid(std::type_index type, handle_t index) const {
		if (!shelves.count(type))
			return false;

		auto shelf = shelves.at(type).get();
		return shelf->size() > index;
	}

	template <typename T> void reserve(std::size_t count) {
		auto& shelf = getshelf<T>();
		shelf.reserve(count);
	}

	template <typename T, typename ... Args> handle_t create(Args&& ... args) {
		auto& shelf = getshelf<T>();
		handle_t handle = 0;
		shelf.allocate(1, &handle);
		shelf[handle] = T(std::forward<Args>(args) ...);
		return handle;
	}

	template <typename T> inline T& get(handle_t index) {
		return getshelf<T>()[index];
	}

	template <typename T> inline const T& get(handle_t index) const {
		return getshelf<T>()[index];
	}

	template <typename T> void release(handle_t index) {
		auto& shelf = getshelf<T>();
		shelf.release(1, &index);
	}

	template <typename T> bool isvalid(handle_t index) const {
		return isvalid(std::type_index(typeid(T)), index);
	}

	template <typename T, typename Func> void foreach(const Func& func) {
		auto& shelf = getshelf<T>();
		for (auto& e : shelf)
			func(e);
	}

	template <typename T> Shelf<T>& getshelf() {
		auto tid = std::type_index(typeid(T));
		if (!shelves.count(tid))
			shelves[tid] = std::make_unique<Shelf<T>>();

		return *(Shelf<T>*) shelves[tid].get();
	}
};

template <typename T>
T& Shelf<T>::at(const handle_t& handle) {
	assert(handle < sparse_set.size() && "index out of range");
	assert(sparse_set.at(handle) < item_set.size() && "sparser index out of range");
	return item_set.at(sparse_set.at(handle));
}

template <typename T>
const T& Shelf<T>::at(const handle_t& handle) const {
	assert(handle < sparse_set.size() && "index out of range");
	assert(sparse_set.at(handle) < item_set.size() && "sparser index out of range");
	return item_set.at(sparse_set.at(handle));
}

template <typename T>
handle_t Shelf<T>::allocate() {
	handle_t handle = {0};

	if (freelist_empty()) {
		handle = sparse_set.size();
		sparse_set.push_back(item_set.size());
	} else {
		handle_t outer = freelist_front;
		handle_t& inner_handle = sparse_set[freelist_front];
		freelist_front = inner_handle;
		if (freelist_empty())
			freelist_back = freelist_front;

		inner_handle = item_set.size();
		handle = outer;
	}

	const auto size = item_set.size();
	item_set.resize(size + 1);
	reverse_set.push_back(handle);
	return handle;
}

template <typename T>
void Shelf<T>::release(const handle_t& handle) {
	if (handle == handle_null || sparse_set.at(handle) >= item_set.size())
		return;

	handle_t inner_handle = sparse_set[handle];
	sparse_set[handle] = handle_null;
	if (freelist_empty()) {
		freelist_front = handle;
		freelist_back = freelist_front;
	} else {
		sparse_set[freelist_back] = handle;
		freelist_back = handle;
	}

	if (item_set.size() > 1) {
		std::swap(item_set[inner_handle], item_set.back());
		std::swap(reverse_set[inner_handle], reverse_set.back());
		sparse_set[reverse_set[inner_handle]] = inner_handle;
	}

	item_set.pop_back();
	reverse_set.pop_back();
}

template <typename T>
void Shelf<T>::allocate(std::size_t count, handle_t* handles) {
	const std::size_t size = item_set.size();
	//item_set.reserve(size + count);
	//reverse_set.reserve(size + count);
	for (std::size_t i = 0; i < count; i++)
		handles[i] = allocate();
}

template <typename T>
void Shelf<T>::release(std::size_t count, handle_t* handles) {
	for (std::size_t i = 0; i < count; i++)
		release(handles[i]);
}