# NextViper Open-Source License Policy

**Project Maintainer**: Nuratix LLC ([nuratix.com](https://nuratix.com))  
**Official Website**: [nextviper.nuratix.com](https://nextviper.nuratix.com)  
**Primary License**: Apache License, Version 2.0  
**SPDX-License-Identifier**: `Apache-2.0`

---

## 1. Core Principles & Intended Policy

NextViper is developed as a free, open-source programming language. The fundamental policy governing the project is:

1. **Free to Use**: Anyone in the world can download, install, and execute NextViper without paying fees or seeking prior permission.
2. **Commercial & Private Freedom**: Developers, startups, enterprises, and researchers may build proprietary, commercial, or open-source software, services, packages, and applications with NextViper without paying royalties.
3. **Open Source & Extensibility**: The full source code of the compiler, runtime, interpreter, standard library, and developer tooling is available for inspection, modification, and community enhancement.
4. **Permissive Contribution Model**: Community members can submit improvements, bug fixes, and feature proposals via pull requests.
5. **Copyright & Governance**: Nuratix LLC remains the copyright holder of its original project code, documentation, and associated assets, stewarding the project on behalf of the developer community.

---

## 2. Evaluation of Established OSI-Approved Permissive Licenses

To select the most appropriate license, Nuratix LLC evaluated three primary OSI-approved open-source licenses:

### 2.1 MIT License
- **Pros**: Extremely concise, widely recognized, low barrier to comprehension.
- **Cons**: Lacks an express patent license grant; does not explicitly protect trademarks or govern patent retaliation in large corporate ecosystems.

### 2.2 BSD 3-Clause License
- **Pros**: Simple permissive terms with an explicit non-endorsement clause.
- **Cons**: Lacks explicit patent license grants found in modern systems languages.

### 2.3 Apache License, Version 2.0 (Selected Standard)
- **Pros**:
  - **Permissive Freedom**: Grants irrevocable, perpetual, worldwide rights to use, modify, and distribute the software.
  - **Explicit Patent Grant**: Every contributor automatically grants a patent license covering their contributions, protecting users against patent infringement claims.
  - **Trademark Distinction**: Clearly delineates software code licensing from trademark ownership (protecting the `NextViper` and `Nuratix` marks from unauthorized impersonation).
  - **Compatibility**: Widely adopted by tier-one systems and compiler ecosystems (e.g. LLVM, Swift, Kubernetes, Android Open Source Project, Apache Arrow).

---

## 3. Downstream Developer Rights

Users who write code in NextViper or compile programs using the NextViper compiler are **NOT** subject to any licensing restrictions on their own output:

- **Your Code is Yours**: Code written in `.nv` source files belongs exclusively to you or your organization.
- **No Viral / Copyleft Restrictions**: NextViper does not impose GPL-style viral obligations on software created using the language.
- **Distribution of Binaries**: Binaries compiled with `nextviper build` or interpreted with `nextviper run` may be distributed under any license of your choosing (commercial, proprietary, MIT, GPL, etc.).

---

## 4. Trademarks and Branding

The Apache 2.0 license grants permissions to use the software code, but does **not** grant trademark rights:
- "NextViper", the NextViper logo, and "Nuratix" are trademarks and brand assets of Nuratix LLC.
- You may freely use the name "NextViper" to refer to the language (e.g., "Built with NextViper", "NextViper package for linear algebra").
- You may not use the name or logo in a manner that falsely implies official sponsorship, endorsement, or ownership by Nuratix LLC without written permission.
