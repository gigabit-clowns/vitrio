// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

// Tuning knobs of the library. Every one of them has a default that is
// reasonable on an ordinary machine and may be overridden from the build,
// which is what makes them worth defining rather than writing down at the
// point of use.

// Widest gap between two stretches of a file that is still worth asking for
// in one piece rather than leaving to the fault that reaches it.
//
// The readahead a kernel does of its own accord is about this wide, so
// bridging a gap wider than it is speculation. What a batch actually bridges
// scales with what one of its regions spans and is only capped here, so that
// a gap wider than the data on either side of it is never bridged: a flat cap
// would have two small regions far apart ask for many times what they hold.
#ifndef VITRIO_PREFETCH_GAP_CAP
	#define VITRIO_PREFETCH_GAP_CAP (2UL<<16)
#endif

// Bytes one step of a read asks for before the regions of the step before it
// are walked.
//
// A batch that fits in this is asked for in one go, which is what a batch of
// small scattered regions wants: all of it is in flight before any of it is
// read, so every latency overlaps. Past it a batch is walked a step at a
// time, so that a read whose regions are large enough that asking for all of
// them at once would be asking for more than can be held asks for them as it
// reaches them instead.
//
// This is per read, so several reads at once hold several of these.
#ifndef VITRIO_PREFETCH_BYTE_BUDGET
	#define VITRIO_PREFETCH_BYTE_BUDGET (4UL<<24)
#endif

// Bytes one step of a read stages in memory of its own, where its values have
// to be converted on their way to the array.
//
// A step is read while the one before it is moved, so a read holds two steps
// of this at most, unless a single stretch is wider than a step.
#ifndef VITRIO_STAGING_BYTE_BUDGET
	#define VITRIO_STAGING_BYTE_BUDGET (1UL<<23)
#endif

// Reads of files the kernel carries out at once for one asynchronous queue or
// port.
//
// A read of scattered regions has this many stretches of the file in flight:
// enough for a solid-state drive to serve them in parallel, and for the
// scheduler of a disk to order them along its way. Past it, queueing a read
// waits for an earlier one.
#ifndef VITRIO_FILE_READ_QUEUE_CAPACITY
	#define VITRIO_FILE_READ_QUEUE_CAPACITY 64
#endif

// Threads that carry out the transfers of files of one file_transfer_pool, each
// with a port of its own.
//
// A thread drives many transfers at once, and only queues their reads and
// hands their conversions to the executor of computations, so one keeps up
// with what the kernel delivers.
#ifndef VITRIO_FILE_TRANSFER_WORKERS
	#define VITRIO_FILE_TRANSFER_WORKERS 1
#endif
