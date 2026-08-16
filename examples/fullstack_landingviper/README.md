<p align="center">
  <a href="https://nextviper.nuratix.com">
    <img src="https://nextviper.nuratix.com/logo-black.png" alt="NextViper Logo" width="180" height="auto" />
  </a>
</p>

<p align="center">
  <a href="https://nuratix.com">
    <img src="https://www.nuratix.com/nuratix-logo-light.png" alt="By Nuratix LLC" width="140" height="auto" />
  </a>
</p>

# landingviper

**Full-Stack Web Application** with a **React Frontend** and a **NextViper Backend REST API & Compute Engine**, developed and maintained by **Nuratix LLC** ([https://nuratix.com](https://nuratix.com)).

NextViper powers the backend services, handling REST API routing, high-performance tensor linear algebra, columnar DataFrame queries, and code execution evaluation.

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    REACT / JS FRONTEND                      │
│   • Interactive Matrix Compute UI                           │
│   • Live Columnar DataFrame Data Table                      │
│   • Real-Time NextViper Code Sandbox                        │
└──────────────────────────────┬──────────────────────────────┘
                               │ JSON REST API
                               ▼
┌─────────────────────────────────────────────────────────────┐
│                NEXTVIPER BACKEND REST SERVER                │
│   • GET  /api/status          (System Telemetry & Health)   │
│   • GET  /api/tensor/compute  (N-Dim Tensor Linear Algebra) │
│   • GET  /api/data/analytics  (Columnar Tabular Analytics)  │
│   • POST /api/execute         (Code Evaluation Worker)      │
└─────────────────────────────────────────────────────────────┘
```

---

## 🚀 Getting Started

### 1. Clone & Run

```bash
git clone https://github.com/nuratix/landingviper.git
cd landingviper

# Start the NextViper Backend & React Full-Stack Server
nextviper run server.nv
```

Or via `npx`:
```bash
npx nextviper run server.nv
```

### 2. Access the Application

Open your browser at:
👉 **[http://localhost:8080](http://localhost:8080)** (or `http://127.0.0.1:8080`)

---

## 📁 Project Structure

```
landingviper/
├── backend/
│   ├── app.nv           # NextViper REST API router and server dispatcher
│   └── api_handlers.nv  # NextViper compute, tensor matmul & dataframe logic
├── frontend/
│   └── index.html       # Dynamic React 18 frontend communicating with backend
├── server.nv            # Master server orchestrator
├── main.nv              # Application entrypoint
├── nextviper.toml       # Package manifest
├── .gitattributes       # GitHub Linguist language definition
└── README.md            # Architecture & Documentation
```

---

## 📜 License

Apache-2.0 © 2026 Nuratix LLC.
