/*
 * \brief  Allocator infrastructure for core
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2009-10-12
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__CORE_MEM_ALLOC_H_
#define _CORE__INCLUDE__CORE_MEM_ALLOC_H_

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/mutex.h>

/* core includes */
#include <synced_range_allocator.h>
#include <util.h>

namespace Core {

	class Core_mem_translator;
	class Core_mem_allocator;

	struct Metadata;
	class Mapped_mem_allocator;
	class Mapped_avl_allocator;

	using Page_allocator = Allocator_avl_tpl<Empty, get_page_size()>;
	using Phys_allocator = Synced_range_allocator<Page_allocator>;
	using Synced_mapped_allocator =
		Synced_range_allocator<Mapped_avl_allocator>;
};


/**
 * Interface of an allocator that allows to return physical addresses
 * of its used virtual address ranges, and vice versa.
 */
class Core::Core_mem_translator : public Range_allocator
{
	public:

		/**
		 * Returns physical address for given virtual one
		 *
		 * \param addr  virtual address
		 */
		virtual void * phys_addr(void * addr) = 0;

		/**
		 * Returns virtual address for given physical one
		 *
		 * \param addr  physical address
		 */
		virtual void * virt_addr(void * addr) = 0;
};


/**
 * Metadata for allocator blocks that stores a related address
 */
struct Core::Metadata { void * map_addr; };


/**
 * Page-size granular allocator that links ranges to related ones.
 */
class Core::Mapped_avl_allocator : public Allocator_avl_tpl<Metadata, get_page_size()>
{
	friend class Mapped_mem_allocator;

	public:

		/**
		 * Constructor
		 *
		 * \param md_alloc  metadata allocator
		 */
		explicit Mapped_avl_allocator(Allocator *md_alloc)
		: Allocator_avl_tpl<Metadata, get_page_size()>(md_alloc) {}

		/**
		 * Returns related address for allocated range
		 *
		 * \param addr  address within allocated range
		 */
		void * map_addr(void * addr);
};


/**
 * Unsynchronized allocator for core-mapped memory
 *
 * This is an allocator of core-mapped memory. It is meant to be used as
 * meta-data allocator for the other allocators and as back end for core's
 * synchronized memory allocator.
 */
class Core::Mapped_mem_allocator : public Core_mem_translator
{
	private:

		Mapped_avl_allocator *_phys_alloc;
		Mapped_avl_allocator *_virt_alloc;

		/*
		 * Noncopyable
		 */
		Mapped_mem_allocator(Mapped_mem_allocator const &);
		Mapped_mem_allocator &operator = (Mapped_mem_allocator const &);

	public:

		/**
		 * Constructor
		 *
		 * \param phys_alloc  allocator of physical memory
		 * \param virt_alloc  allocator of core-local virtual memory ranges
		 */
		Mapped_mem_allocator(Synced_mapped_allocator &phys_alloc,
		                     Synced_mapped_allocator &virt_alloc)
		: _phys_alloc(&phys_alloc._alloc), _virt_alloc(&virt_alloc._alloc) { }

		/**
		 * Establish mapping between physical and virtual address range
		 *
		 * Note: has to be implemented by platform specific code
		 *
		 * \param virt_addr  start address of virtual range
		 * \param phys_addr  start address of physical range
		 * \param size       size of range
		 */
		bool _map_local(addr_t virt_addr, addr_t phys_addr, size_t size);

		/**
		 * Destroy mapping between physical and virtual address range
		 *
		 * Note: has to be implemented by platform specific code
		 *
		 * \param virt_addr  start address of virtual range
		 * \param phys_addr  start address of physical range
		 * \param size       size of range
		 */
		bool _unmap_local(addr_t virt_addr, addr_t phys_addr, size_t size);


		/***********************************
		 ** Core_mem_translator interface **
		 ***********************************/

		void * phys_addr(void * addr) override {
			return _virt_alloc->map_addr(addr); }

		void * virt_addr(void * addr) override {
			return _phys_alloc->map_addr(addr); }


		/*******************************
		 ** Range allocator interface **
		 *******************************/

		Range_result add_range(addr_t, size_t)    override { return Alloc_error::DENIED; }
		Range_result remove_range(addr_t, size_t) override { return Alloc_error::DENIED; }
		Alloc_result alloc_aligned(size_t, unsigned, Range) override;
		Alloc_result alloc_addr(size_t, addr_t) override { return Alloc_error::DENIED; }
		void free(void *) override;
		size_t avail() const override { return _phys_alloc->avail(); }
		bool valid_addr(addr_t addr) const override {
			return _virt_alloc->valid_addr(addr); }

		using Range_allocator::alloc_aligned; /* import overloads */


		/*********************************
		 ** Memory::Allocator interface **
		 *********************************/

		Alloc_result try_alloc(size_t size) override {
			return alloc_aligned(size, (unsigned)log2(sizeof(addr_t))); }

		void _free(Allocation &a) override { free(a.ptr, a.num_bytes); }


		/****************************************
		 ** Legacy Genode::Allocator interface **
		 ****************************************/

		void   free(void *addr, size_t) override;
		size_t consumed() const override { return _phys_alloc->consumed(); }
		size_t overhead(size_t size) const override {
			return _phys_alloc->overhead(size); }
		bool   need_size_for_free() const override {
			return _phys_alloc->need_size_for_free(); }
};


/**
 * Allocators for physical memory, core's virtual address space,
 * and core-local memory. The interface of this class is thread safe.
 * The class itself implements a ready-to-use memory allocator for
 * core that allows to allocate memory at page granularity only.
 */
class Core::Core_mem_allocator : public Core_mem_translator
{
	protected:

		/**
		 * Mutex used for synchronization of all operations on the
		 * embedded allocators.
		 */
		Mutex _mutex { };

		/**
		 * Synchronized allocator of physical memory ranges
		 *
		 * This allocator must only be used to allocate memory
		 * ranges at page granularity.
		 */
		Synced_mapped_allocator _phys_alloc;

		/**
		 * Synchronized allocator of core's virtual memory ranges
		 *
		 * This allocator must only be used to allocate memory
		 * ranges at page granularity.
		 */
		Synced_mapped_allocator _virt_alloc;

		/**
		 * Unsynchronized core-mapped memory allocator
		 *
		 * This allocator is internally used within this class for
		 * allocating meta data for the other allocators. It is not
		 * synchronized to avoid nested locking. The Mutex-guarded
		 * access to this allocator from the outer world is
		 * provided via the 'Allocator' interface implemented by
		 * 'Core_mem_allocator'. The allocator works at byte
		 * granularity.
		 */
		Mapped_mem_allocator _mem_alloc;

	public:

		/**
		 * Constructor
		 */
		Core_mem_allocator()
		: _phys_alloc(_mutex, &_mem_alloc),
		  _virt_alloc(_mutex, &_mem_alloc),
		  _mem_alloc(_phys_alloc, _virt_alloc) { }

		/**
		 * Access physical-memory allocator
		 */
		Synced_mapped_allocator &phys_alloc() { return _phys_alloc; }

		/**
		 * Access core's virtual-memory allocator
		 */
		Synced_mapped_allocator &virt_alloc() { return _virt_alloc; }


		/***********************************
		 ** Core_mem_translator interface **
		 ***********************************/

		void * phys_addr(void * addr) override
		{
			return _virt_alloc()->map_addr(addr);
		}

		void * virt_addr(void * addr) override
		{
			return _phys_alloc()->map_addr(addr);
		}


		/*******************************
		 ** Range allocator interface **
		 *******************************/

		Range_result add_range(addr_t, size_t)    override { return Alloc_error::DENIED; }
		Range_result remove_range(addr_t, size_t) override { return Alloc_error::DENIED; }
		Alloc_result alloc_addr(size_t, addr_t)   override { return Alloc_error::DENIED; }

		Alloc_result alloc_aligned(size_t size, unsigned align, Range range) override
		{
			Mutex::Guard lock_guard(_mutex);
			return _mem_alloc.alloc_aligned(size, align, range);
		}

		void free(void *addr) override
		{
			Mutex::Guard lock_guard(_mutex);
			return _mem_alloc.free(addr);
		}

		size_t avail() const override { return _phys_alloc.avail(); }

		bool valid_addr(addr_t addr) const override { return _virt_alloc.valid_addr(addr); }

		using Range_allocator::alloc_aligned; /* import overloads */


		/*********************************
		 ** Memory::Allocator interface **
		 *********************************/

		Alloc_result try_alloc(size_t size) override
		{
			return alloc_aligned(size, (unsigned)log2(sizeof(addr_t)));
		}

		void _free(Allocation &a) override { free(a.ptr, a.num_bytes); }


		/****************************************
		 ** Legacy Genode::Allocator interface **
		 ****************************************/

		void free(void *addr, size_t size) override
		{
			Mutex::Guard lock_guard(_mutex);
			return _mem_alloc.free(addr, size);
		}

		size_t consumed()            const override { return _phys_alloc.consumed(); }
		size_t overhead(size_t size) const override { return _phys_alloc.overhead(size); }

		bool need_size_for_free() const override {
			return _phys_alloc.need_size_for_free(); }
};

#endif /* _CORE__INCLUDE__CORE_MEM_ALLOC_H_ */
