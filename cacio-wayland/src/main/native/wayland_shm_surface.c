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

#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "wayland_shm_surface.h"

static void noop() {
	// TODO: FIXME
}

static void xdg_surface_handle_configure(void *data,
		struct xdg_surface *xdg_surface, uint32_t serial) {
	xdg_surface_ack_configure(xdg_surface, serial);
	//wl_surface_commit(surface);
}

static struct xdg_surface_listener surface_listener = {
	.configure = xdg_surface_handle_configure,
};

static struct xdg_toplevel_listener toplevel_listener = {
	.configure = noop,
	.close = noop,
};

static struct wl_surface* make_surface() {
    struct display* display = get_display();
    return wl_compositor_create_surface(display->compositor);
}

static struct xdg_surface* make_shell_surface(struct wl_surface* surface) {
    struct display* display = get_display();
		struct xdg_surface *xdg_surface = xdg_wm_base_get_xdg_surface(display->wm_base, surface);
		struct xdg_toplevel *xdg_toplevel = xdg_surface_get_toplevel(xdg_surface);

		xdg_surface_add_listener(xdg_surface, &surface_listener, NULL);
		xdg_toplevel_add_listener(xdg_toplevel, &toplevel_listener, NULL);

		wl_surface_commit(surface);
		wl_display_roundtrip(display->display);
    return xdg_surface;
}

static struct wl_shm_pool* make_shm_pool(int32_t width, int32_t height, int32_t pixel_depth, void** addr) {
  struct wl_shm_pool  *pool;
  char template[] = "/tmp/wayland_mmap_XXXXXX";
  int fd;

   int32_t stride = width * pixel_depth;
   int32_t size = stride * height;

   fd = mkostemp(template, O_RDWR | O_CREAT | O_TRUNC);
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
    struct wl_surface*          surface;
    struct xdg_surface*    shell_surface;

    struct wl_shm_pool*         pool;
    struct wl_buffer*           buffer;
    void*                       content;

    surface = make_surface();
    if (surface == NULL) return NULL;

    shell_surface = make_shell_surface(surface);
    if (shell_surface == NULL) {
        wl_surface_destroy(surface);
        return NULL;
    }

    pool = make_shm_pool(get_display_width(), get_display_height(), pixel_depth, &content);
    if (pool == NULL) {
        wl_surface_destroy(surface);
     //FIXME   wl_shell_surface_destroy(shell_surface);
        return NULL;
    }

    buffer = make_buffer(pool, width, height, WL_SHM_FORMAT_XRGB8888, pixel_depth);
    if (buffer == NULL) {
        wl_shm_pool_destroy(pool);
        wl_surface_destroy(surface);
    //FIXME    wl_shell_surface_destroy(shell_surface);
        return NULL;
    }


    struct shm_surface* ssf = (struct shm_surface*)malloc(sizeof(struct shm_surface));
    if (ssf == NULL) {
        wl_buffer_destroy(buffer);
        wl_shm_pool_destroy(pool);
        wl_surface_destroy(surface);
    //FIXME    wl_shell_surface_destroy(shell_surface);
        return NULL;
    }

    memset((void*)ssf, 0, sizeof(struct shm_surface));

    ssf->surface = surface;
    ssf->xdg_surface = shell_surface;
    ssf->buffer = buffer;
    ssf->pool = pool;

    ssf->content = content;
    ssf->id = id;
    ssf->x = x;
    ssf->y = y;
    ssf->width = width;
    ssf->height = height;
    ssf->pixel_depth = pixel_depth;
    ssf->format = WL_SHM_FORMAT_XRGB8888;

    wl_surface_set_user_data(surface, ssf);

    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_commit(surface);

    return ssf;
}

void DestroyShmScreenSurface(ShmSurface* surf) {
    struct input* input = get_input();
    input->activeSurface = NULL;

    wl_buffer_destroy(surf->buffer);
    wl_shm_pool_destroy(surf->pool);
    wl_surface_destroy(surf->surface);
    xdg_surface_destroy(surf->xdg_surface);

    free(surf);
}


void ResizeShmScreenSurface(ShmSurface* surf, int32_t width, int32_t height) {
    UnmapShmScreenSurface(surf);
    RemapShmScreenSurface(surf, width, height);
}

void UnmapShmScreenSurface(ShmSurface* surf) {
    wl_surface_attach(surf->surface, NULL, 0, 0);
    wl_surface_commit(surf->surface);
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
