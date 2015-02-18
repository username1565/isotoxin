/*
    Rectangles UI module
    (C) 2015 ROTKAERMOTA (TOX: ED783DA52E91A422A48F984CAC10B6FF62EE928BE616CE41D0715897D7B7F6050B2F04AA02A1)
*/
#pragma once

#pragma warning (push)
#pragma warning (disable:4820)
#pragma warning (disable:4100)
#include <memory>
#include "fastdelegate/FastDelegate.h"
#pragma warning (pop)

#include "toolset/toolset.h"
#include "system/system.h"

#define SLASSERT ASSERTO
#define SLERROR ERROR
#include "spinlock/spinlock.h"
#include "spinlock/queue.h"

#define MWHEELPIXELS 20

#include "menu.h"
#include "rtools.h"
#include "gmesages.h"
#include "theme.h"
#include "guirect.h"
#include "rectengine.h"
#include "gui.h"
#include "dialog.h"