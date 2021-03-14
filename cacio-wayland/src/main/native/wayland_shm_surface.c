/*
 * Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#define _GNU_SOURCE
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "wayland_events.h"
#include "wayland_shm_surface.h"

static void noop() {
	// TODO: FIXME
}

static void xdg_surface_handle_configure(void *data,
		struct xdg_surface *xdg_surface, uint32_t serial) {
	xdg_surface_ack_configure(xdg_surface, serial);

    ShmSurface *ssf = data;
    new_surface_event(SURFACE_RESIZE, ssf, ssf->width, ssf->height);
    ssf->configured = true;
}

static struct xdg_surface_listener surface_listener = {
	.configure = xdg_surface_handle_configure,
};

static void xdg_toplevel_handle_configure(void *data, struct xdg_toplevel *toplevel,
        int32_t width, int32_t height, struct wl_array * states) {
    ShmSurface *ssf = data;
    if (width > 0 && height > 0) {
        ssf->width = width;
        ssf->height = height;
    }
}

static struct xdg_toplevel_listener toplevel_listener = {
	.configure = xdg_toplevel_handle_configure,
	.close = noop,
};

static struct wl_surface* make_surface() {
    struct display* display = get_display();
    return wl_compositor_create_surface(display->compositor);
}

static struct xdg_surface* make_shell_surface(struct wl_surface* surface, ShmSurface *ssf) {
    struct display* display = get_display();
		struct xdg_surface *xdg_surface = xdg_wm_base_get_xdg_surface(display->wm_base, surface);
		struct xdg_toplevel *xdg_toplevel = xdg_surface_get_toplevel(xdg_surface);

		xdg_surface_add_listener(xdg_surface, &surface_listener, ssf);
		xdg_toplevel_add_listener(xdg_toplevel, &toplevel_listener, ssf);

		wl_surface_commit(surface);

        while (!ssf->configured)
		  wl_display_roundtrip(display->display);
    return xdg_surface;
}

static struct wl_shm_pool* make_shm_pool(int32_t width, int32_t height, int32_t pixel_depth, void** addr) {
  struct wl_shm_pool  *pool;
  int fd;

   int32_t stride = width * pixel_depth;
   int32_t size = stride * height;

   fd = memfd_create("wayland-shm-pool", 0);
   if (fd < 0) {
     return NULL;
   }

   if (ftruncate(fd, (size_t)size) < 0) {
     return NULL;
   }

   void* map = mmap(NULL, (size_t)size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
   if (map == MAP_FAILED) {
     close(fd);
     return NULL;
   }

  *addr = map;

   pool = wl_shm_create_pool(get_display()->shm, fd, size);

   return pool;
}

static struct wl_buffer* make_buffer(struct wl_shm_pool *pool, int32_t width, int32_t height, uint32_t format, int32_t pixel_depth) {
  return wl_shm_pool_create_buffer(pool, 0, width, height, width * pixel_depth, format);
}

ShmSurface* CreateShmScreenSurface(int64_t id, int32_t x, int32_t y, int32_t width, int32_t height, int32_t pixel_depth) {
    void *content;

    ShmSurface *ssf = malloc(sizeof(ShmSurface));
    if (!ssf) return NULL;
    memset(ssf, 0, sizeof(ShmSurface));
    ssf->width = width;
    ssf->height = height;

    ssf->surface = make_surface();
    if (ssf->surface == NULL) {
        free(ssf);
        return NULL;
    }

    ssf->xdg_surface = make_shell_surface(ssf->surface, ssf);
    if (ssf->xdg_surface == NULL) {
        wl_surface_destroy(ssf->surface);
        free(ssf);
        return NULL;
    }

    ssf->pool = make_shm_pool(get_display_width(), get_display_height(), pixel_depth, &content);
    if (ssf->pool == NULL) {
        wl_surface_destroy(ssf->surface);
        xdg_surface_destroy(ssf->xdg_surface);
        free(ssf);
        return NULL;
    }

    ssf->buffer = make_buffer(ssf->pool, ssf->width, ssf->height, WL_SHM_FORMAT_XRGB8888, pixel_depth);
    if (ssf->buffer == NULL) {
        wl_shm_pool_destroy(ssf->pool);
        wl_surface_destroy(ssf->surface);
        xdg_surface_destroy(ssf->xdg_surface);
        free(ssf);
        return NULL;
    }

    ssf->content = content;
    ssf->id = id;
    ssf->x = x;
    ssf->y = y;
    ssf->width = width;
    ssf->height = height;
    ssf->pixel_depth = pixel_depth;
    ssf->format = WL_SHM_FORMAT_XRGB8888;

    wl_surface_set_user_data(ssf->surface, ssf);

    wl_surface_attach(ssf->surface, ssf->buffer, 0, 0);
    wl_surface_commit(ssf->surface);
    ssf->ready = true;

    return ssf;
}

void DestroyShmScreenSurface(ShmSurface* surf) {
    struct input* input = get_input();
    input->activeSurface = NULL;

    wl_buffer_destroy(surf->buffer);
    wl_shm_pool_destroy(surf->pool);
    xdg_surface_destroy(surf->xdg_surface);
    wl_surface_destroy(surf->surface);

    free(surf);
}

void UnmapShmScreenSurface(ShmSurface* surf) {
    wl_surface_attach(surf->surface, NULL, 0, 0);
    wl_surface_commit(surf->surface);
    if (surf->buffer)
        wl_buffer_destroy(surf->buffer);
    surf->buffer = NULL;
    surf->width = 0;
    surf->height = 0;
}


bool RemapShmScreenSurface(ShmSurface* surface, int32_t width, int32_t height) {
    struct wl_buffer* buffer = make_buffer(surface->pool, width, height, WL_SHM_FORMAT_XRGB8888, surface->pixel_depth);
    if (buffer == NULL) {
        return false;
    }
    surface->width = width;
    surface->height = height;

    wl_surface_attach(surface->surface, buffer, 0, 0);
    wl_surface_damage(surface->surface, 0, 0, width, height);
    wl_surface_commit(surface->surface);
    surface->buffer = buffer;
    return true;
}
