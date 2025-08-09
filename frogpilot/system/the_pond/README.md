# The Pond

**The Pond** is a lightweight web-based interface for managing your device. It allows you to adjust settings remotely, view video streams, and download logs—all from your browser.

---

# Architecture

The Pond has two main components:  
- A **Python Flask API**  
- A **frontend built with Arrow.js** (a minimal reactive framework)

Because the frontend uses native ES modules, there's no need for a build step or Node.js tooling.

## API

The API is simple and defined in `the_pond.py`. It exposes a handful of JSON-based endpoints via standard Flask routes:

- Get and update settings  
- Fetch error logs  
- List recorded routes  
- Load a specific route and video stream (ported from Fleet Manager)  
- Save a navigation destination (triggers OpenPilot navigation)

## Web App

The frontend is built using [Arrow.js](https://www.arrow-js.com), chosen for its simplicity and native module support (no bundlers required). It includes the following main components:

- **Error Logs**
- **Navigation**
- **Routes**
- **Settings**

It also includes some layout-related components:

- `"router"` – for dynamic view switching
- `"sidebar"` – for navigation

---

# Development Guide

## Adding a New Page

To add a new page:

1. **Create the component**  
   Add a new file in the `components/` directory.

2. **Register the route**  
   Add your route to `router.js`. The router uses a simple list of route definitions:

    ```js
    let routes = [
      createRoute("errorLogs", "/error_logs", ErrorLogs),
      createRoute("navdestination", "/navigation", NavDestination),
      createRoute("root", "/", Home),
      createRoute("route", "/routes/:routeDate", RecordedRoute),
      createRoute("routes", "/routes", RecordedRoutes),
      createRoute("settings", "/settings/:section/:subsection?", SettingsView),
      createRoute("NAME", "/URL_PATH", ComponentName), <-- Insert your component here
    ];
    ```

3. **Add to sidebar**  
   Modify `sidebar.js` to include your new route:

    ```js
    const MenuItems = {
      ...
      navigation: [
        {
          name: "Set destination",
          link: "/navigation",
          icon: "bi-globe-americas",
        },
        {
          name: "Sidebar title", <-- Add your new page here
          link: "/URL_PATH", <-- Same url as in router.js
          icon: "" <-- Add an icon here (Bootstrap icons are used, see https://icons.getbootstrap.com/
        },
      ],
      ...
    }
    ```

4. **Create the component**  
   Here's a simple example component:

    ```js
    import { html, reactive } from "https://esm.sh/@arrow-js/core"

    export function MyComponent() {
      const state = reactive({
        message: "Hello from MyComponent"
      })

      return html`
        <div>
          <h1>${state.message}</h1>
        </div>
      `
    }
    ```

---

# Running The Pond

### Using Docker

```bash
docker build -t the_pond .
docker run -v $(pwd):/app --rm -ti -p 8084:8084 the_pond
```

### Run and debug on comma device (or computer with python)

```bash
./start.sh
```
