// Copyright (c) 2017-2026, Lawrence Livermore National Security, LLC and other CEED contributors.
// All Rights Reserved. See the top-level LICENSE and NOTICE files for details.
//
// SPDX-License-Identifier: BSD-2-Clause
//
// This file is part of CEED:  http://github.com/ceed

// This file and definition is used for in-source builds.
// The definition for installs is in ceed-jit-source-root-install.c.
const char *CeedJitSourceRootDefault = CEED_JIT_SOURCE_ROOT_DEFAULT;

#if defined (CEED_JIT_CUTE_SOURCE_ROOT_DEFAULT)
const char *CeedJitCuteSourceRootDefault = CEED_JIT_CUTE_SOURCE_ROOT_DEFAULT;
#else
const char *CeedJitCuteSourceRootDefault = CEED_JIT_SOURCE_ROOT_DEFAULT;
#endif

#if defined (CEED_JIT_CUDA_SOURCE_ROOT_DEFAULT)
const char *CeedJitCudaSourceRootDefault = CEED_JIT_CUDA_SOURCE_ROOT_DEFAULT;
#else
const char *CeedJitCudaSourceRootDefault = CEED_JIT_SOURCE_ROOT_DEFAULT;
#endif
