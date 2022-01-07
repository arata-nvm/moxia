#pragma once
#include <stddef.h>

const size_t kPageDirectoryCount = 64;

void SetupIdentityPageTable();

void InitializePaging();
