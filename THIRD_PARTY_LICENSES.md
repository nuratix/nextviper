# NextViper Third-Party Licenses & Attributions

NextViper utilizes several open-source libraries, specifications, and system APIs in its compiler, runtime engine, and web infrastructure. All third-party components and their respective licenses are listed below.

---

## 1. Compiler & Runtime Dependencies

### 1.1 Vulkan Headers & Loader
- **Component**: Vulkan API Headers & Loader (`libvulkan`)
- **Origin**: Khronos Group ([khronos.org/vulkan](https://www.khronos.org/vulkan/))
- **License**: Apache License 2.0 / MIT
- **Description**: Used for cross-platform GPU device enumeration, compute pipeline dispatch, and shader memory management.

### 1.2 POSIX Threads & C++20 Standard Library
- **Component**: `pthread` / ISO C++20 Standard Library (libstdc++ / libc++)
- **Origin**: Free Software Foundation / LLVM Project
- **License**: GPLv3 with GCC Runtime Library Exception / Apache 2.0 with LLVM Exception
- **Description**: Concurrency, memory management, filesystem primitives, and standard containers.

---

## 2. Web Registry & Documentation Subsystems

### 2.1 Next.js
- **Component**: Next.js React Framework
- **Origin**: Vercel, Inc. ([nextjs.org](https://nextjs.org))
- **License**: MIT License
- **Description**: Web application routing, server-side rendering, and static site generation for `nextviper.nuratix.com`.

### 2.2 Lucide React
- **Component**: Lucide Icons
- **Origin**: Lucide Contributors ([lucide.dev](https://lucide.dev))
- **License**: ISC License
- **Description**: UI iconography for documentation, packages, and developer portal.

### 2.3 Tailwind CSS
- **Component**: Tailwind CSS Utility Engine
- **Origin**: Tailwind Labs ([tailwindcss.com](https://tailwindcss.com))
- **License**: MIT License
- **Description**: Web application design token styling.

---

## 3. Compliance and Inquiries

NextViper distributes binary artifacts in full compliance with the aforementioned licenses. For licensing questions or source code requests regarding dynamically linked components, contact `opensource@nuratix.com`.
