/*
 * \brief  Slab allocator implementation
 * \author Norman Feske
 * \date   2006-05-16
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/slab.h>
#include <util/construct_at.h>
#include <util/misc_math.h>

using namespace Genode;


/**
 * A slab block holds an array of slab entries.
 */
class Genode::Slab::Block
{
	friend struct Genode::Slab::Entry;

	public:

		Block *next = this;  /* next block in ring     */
		Block *prev = this;  /* previous block in ring */

	private:

		enum { FREE, USED };

		Slab  &_slab;                              /* back reference to slab     */
		size_t _avail = _slab._entries_per_block;  /* free entries of this block */

		/*
		 * Each slab block consists of three areas, a fixed-size header
		 * that contains the member variables declared above, a byte array
		 * called state table that holds the allocation state for each slab
		 * entry, and an area holding the actual slab entries. The number
		 * of state-table elements corresponds to the maximum number of slab
		 * entries per slab block (the '_entries_per_block' member variable of
		 * the Slab allocator).
		 */

		char _data[0];  /* dynamic data (state table and slab entries) */

		/*
		 * Caution! no member variables allowed below this line!
		 */

		/**
		 * Return the allocation state of a slab entry
		 */
		inline bool _state(int idx) { return _data[idx]; }

		/**
		 * Set the allocation state of a slab entry
		 */
		inline void _state(int idx, bool state) { _data[idx] = state; }

		/**
		 * Request address of slab entry by its index
		 */
		Entry *_slab_entry(int idx);

		/**
		 * Determine block index of specified slab entry
		 */
		int _slab_entry_idx(Entry *e);

		/**
		 * These functions are called by Slab::Entry.
		 */
		void inc_avail(Entry &e);
		void dec_avail() { _avail--; }

	public:

		/**
		 * Constructor
		 */
		explicit Block(Slab &slab) : _slab(slab)
		{
			for (unsigned i = 0; i < _avail; i++)
				_state(i, FREE);
		}

		/**
		 * Request number of available entries in block
		 */
		unsigned avail() const { return (unsigned)_avail; }

		/**
		 * Allocate slab entry from block
		 */
		void *alloc();

		/**
		 * Return a used slab block entry
		 */
		Entry *any_used_entry();
};


struct Genode::Slab::Entry
{
		Block &block;
		char   data[0];

		/*
		 * Caution! no member variables allowed below this line!
		 */

		explicit Entry(Block &block) : block(block)
		{
			block.dec_avail();
		}

		~Entry()
		{
			block.inc_avail(*this);
		}

		bool used() {
			return block._state(block._slab_entry_idx(this)) == Block::USED; }

		/**
		 * Lookup Entry by given address
		 *
		 * The specified address is supposed to point to _data[0].
		 */
		static Entry *slab_entry(void *addr) {
			return (Entry *)((addr_t)addr - sizeof(Entry)); }
};


/****************
 ** Slab block **
 ****************/

Slab::Entry *Slab::Block::_slab_entry(int idx)
{
	/*
	 * The slab slots start after the state array that consists
	 * of 'num_elem' bytes. We align the first slot to a four-aligned
	 * address.
	 */

	int    const alignment   = (int)log2(sizeof(addr_t));
	size_t const slots_start = align_addr(_slab._entries_per_block, alignment);
	size_t const entry_size  = sizeof(Entry) + _slab._slab_size;

	return (Entry *)&_data[slots_start + entry_size*idx];
}


int Slab::Block::_slab_entry_idx(Slab::Entry *e)
{
	size_t const slot_size   = sizeof(Entry) + _slab._slab_size;
	addr_t const slot_offset = ((addr_t)e - (addr_t)_slab_entry(0));

	return (int)(slot_offset / slot_size);
}


void *Slab::Block::alloc()
{
	for (unsigned i = 0; i < _slab._entries_per_block; i++) {
		if (_state(i) != FREE)
			continue;

		_state(i, USED);
		Entry * const e = _slab_entry(i);
		construct_at<Entry>(e, *this);
		return e->data;
	}
	return nullptr;
}


Slab::Entry *Slab::Block::any_used_entry()
{
	for (unsigned i = 0; i < _slab._entries_per_block; i++)
		if (_state(i) == USED)
			return _slab_entry(i);

	return nullptr;
}


void Slab::Block::inc_avail(Entry &e)
{
	/* mark slab entry as free */
	_state(_slab_entry_idx(&e), FREE);
	_avail++;
}


/**********
 ** Slab **
 **********/

Slab::Slab(size_t slab_size, size_t block_size, void *initial_sb,
           Allocator *backing_store)
:
	_slab_size(slab_size),
	_block_size(block_size),

	/*
	 * Calculate number of entries per slab block.
	 *
	 * The 'sizeof(umword_t)' is for the alignment of the first slab entry.
	 * The 1 is for one byte state entry.
	 */
	_entries_per_block((_block_size - sizeof(Block) - sizeof(umword_t))
	                   / (_slab_size + sizeof(Entry) + 1)),
	_initial_sb((Block *)initial_sb),
	_nested(false),
	_backing_store(backing_store)
{
	static_assert(sizeof(Slab::Block) <= overhead_per_block());
	static_assert(sizeof(Slab::Entry) <= overhead_per_entry());
}


Slab::~Slab()
{
	if (!_backing_store)
		return;

	/* free backing store */
	while (_num_blocks > 1) {
		/* never free the initial block */
		if (_curr_sb == _initial_sb)
			_curr_sb = _curr_sb->next;
		_free_curr_sb();
	}

	/* release last block */
	_release_backing_store(_curr_sb);
}


Slab::New_slab_block_result Slab::_new_slab_block()
{
	using Result = New_slab_block_result;

	if (!_backing_store)
		return Alloc_error::DENIED;

	Slab &this_slab = *this;
	return _backing_store->try_alloc(_block_size).convert<Result>(
		[&] (Allocation &sb) {
			sb.deallocate = false;
			return construct_at<Block>(sb.ptr, this_slab); },
		[&] (Alloc_error error) {
			return error; });
}


void Slab::_release_backing_store(Block *block)
{
	if (!block)
		return;

	if (block->avail() != _entries_per_block)
		error("freeing non-empty slab block");

	_total_avail -= block->avail();
	_num_blocks--;

	/*
	 * Free only the slab blocks that we allocated dynamically. This is not the
	 * case for the '_initial_sb' that we got as constructor argument.
	 */
	if (_backing_store && (block != _initial_sb))
		_backing_store->free(block, _block_size);
}


void Slab::_free_curr_sb()
{
	/* block to free */
	Block * const block = _curr_sb;

	/* advance '_curr_sb' because the old pointer won't be valid any longer */
	_curr_sb = _curr_sb->next;

	/* never free the initial block */
	if (_num_blocks <= 1)
		return;

	/* remove block from ring */
	block->prev->next = block->next;
	block->next->prev = block->prev;

	_release_backing_store(block);
}


void Slab::_insert_sb(Block *sb)
{
	sb->prev = _curr_sb;
	sb->next = _curr_sb->next;

	_curr_sb->next->prev = sb;
	_curr_sb->next = sb;

	_total_avail += _entries_per_block;
	_num_blocks++;
}


Slab::Expand_result Slab::_expand()
{
	if (!_backing_store || _nested)
		return Ok();

	/* allocate new block for slab */
	_nested = true;

	/* reset '_nested' when leaving the scope */
	struct Nested_guard {
		bool &_nested;
		Nested_guard(bool &nested) : _nested(nested) { }
		~Nested_guard() { _nested = false; }
	} guard(_nested);

	return _new_slab_block().convert<Expand_result>(

		[&] (Block *sb_ptr) {

			/*
			 * The new block has the maximum number of available slots.
			 * Hence, we can insert it at the beginning of the sorted block
			 * list.
			 */
			_insert_sb(sb_ptr);
			return Ok(); },

		[&] (Alloc_error error) {
			return error; });
}


void Slab::insert_sb(void *ptr)
{
	_insert_sb(construct_at<Block>(ptr, *this));
}


Allocator::Alloc_result Slab::try_alloc(size_t size)
{
	/* too large for us ? */
	if (size > _slab_size) {
		error("requested size ", size, " is larger then slab size ",
		      _slab_size);
		return Alloc_error::DENIED;
	}

	/* init very first block */
	if (!_curr_sb) {
		Alloc_error error = Alloc_error::DENIED;
		if (_initial_sb)
			_curr_sb = _initial_sb;
		else
			_new_slab_block().with_result(
				[&] (Block *sb) { _curr_sb = sb; },
				[&] (Alloc_error e) { error = e; });

		if (!_curr_sb)
			return error;

		construct_at<Block>(_curr_sb, *this);
		_total_avail = _entries_per_block;
		_num_blocks  = 1;
	}

	/*
	 * If we run out of slab, we need to allocate a new slab block. For the
	 * special case that this block is allocated using the allocator that by
	 * itself uses the slab allocator, such an allocation could cause up to
	 * three new slab_entry allocations. So we need to ensure to allocate the
	 * new slab block early enough - that is if there are only three free slab
	 * entries left.
	 */
	if (_total_avail <= 3) {
		Expand_result expand_result = _expand();
		if (expand_result.failed())
			return expand_result.convert<Alloc_result>(
				[&] (Ok)            { return Alloc_error::DENIED; },
				[&] (Alloc_error e) { return e; });
	}

	/* skip completely occupied slab blocks, detect cycles */
	Block const * const orig_curr_sb = _curr_sb;
	for (; _curr_sb->avail() == 0; _curr_sb = _curr_sb->next)
		if (_curr_sb->next == orig_curr_sb)
			break;

	void *ptr = _curr_sb->alloc();
	if (!ptr)
		return Alloc_error::DENIED;

	_total_avail--;

	return { *this, { ptr, _slab_size } };
}


void Slab::_free(void *addr)
{
	Entry *e = addr ? Entry::slab_entry(addr) : nullptr;

	if (!e)
		return;

	if (addr < (void *)((addr_t)&e->block + sizeof(e->block)) ||
	    addr >= (void *)((addr_t)&e->block + _block_size)) {
		error("slab block ", Hex_range<addr_t>((addr_t)&e->block, _block_size),
		      " is corrupt - slab address ", addr);
		return;
	}

	Block &block = e->block;

	if (!e->used()) {
		error("slab address ", addr, " freed which is unused");
		return;
	}

	e->~Entry();
	_total_avail++;

	/*
	 * Release completely free slab blocks if the total number of free slab
	 * entries exceeds the capacity of two slab blocks. This way we keep
	 * a modest amount of available entries around so that thrashing effects
	 * are mitigated.
	 */
	_curr_sb = &block;
	while (_total_avail > 2*_entries_per_block
	 && _num_blocks > 1
	 && _curr_sb->avail() == _entries_per_block
	 && _backing_store) {
		_free_curr_sb();
	}
}


void Slab::free_empty_blocks()
{
	for (size_t blocks = _num_blocks; blocks > 0; blocks--) {
		if (_curr_sb != _initial_sb && _curr_sb->avail() == _entries_per_block)
			_free_curr_sb();
		else
			_curr_sb = _curr_sb->next;
	}
}


void *Slab::any_used_elem()
{
	if (_total_avail == _num_blocks*_entries_per_block)
		return nullptr;

	/*
	 * We know that there exists at least one used element.
	 */

	/* skip completely free slab blocks */
	for (; _curr_sb->avail() == _entries_per_block; _curr_sb = _curr_sb->next);

	/* found a block with used elements - return address of the first one */
	Entry *e = _curr_sb->any_used_entry();

	return e ? e->data : nullptr;
}


size_t Slab::consumed() const { return _num_blocks*_block_size; }
