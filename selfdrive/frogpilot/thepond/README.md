# Thepond

This is a new way to manage your frogpilot device. It allows you to change most settings that you can edit in on the device, as well as view the video stream and download the logs.


# Architecture

ThePond consists of two parts: the python-flask API and the Arrow.js SPA frontend. By using es-modules I've been able to avoid having to introcude any kind of build-system (mostly as that would require node somewhere in the build process, and I have no clue how I would go about adding that :P).

### API

The API is fairly simple, there are endpoints for (these are all found in `thepond.py`, and are implemented as simple flask routes which return json):

- Getting and saving settings
- Fetching lists of driven routes
- Loading a specific route and it's video stream (this implementation was just taken straight from Fleet Manager)
- Fetching error logs
- Saving a navigation destination (which will trigger openpilot to start navigation there)

### Webapp

The Webapp is built using [Arrow.js](https://www.arrow-js.com) (since I wanted to have some sort of reactive framework, and this one seemed simple enough and could be used without webpack or similar). It consists of a few components:

- Navigation
- Settings
- Routes
- Error logs

There are then a few structural components such as the `sidebar`, `router` and `theme-toggle` which are used to manage the layout and navigation of the app.

---

# Developing 

## Add a new page

To add a new page, you need to create a new component in the `components` folder, and then add it to the `router` component. The router component is a simple switch statement which renders the correct component based on the current route:

`router.js`
```js 
 let routes = [
    createRoute("root", "/", Overview),
    createRoute("routes", "/routes", RecordedRoutes),
    createRoute("route", "/routes/:routeDate", RecordedRoute),
    createRoute("settings", "/settings/:section/:subsection?", SettingsView),
    createRoute("navdestination", "/navigation", NavDestination),
    createRoute("errorLogs", "/error-logs", ErrorLogs),
    createRoute("NAME", "/URL_PATH", ComponentName), <-- Insert your component here
  ];
  ]
```


Then add your new page to the sidebar, in the appropriate section (or in a new section if you prefer):

`sidebar.js`
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

Once you've added your new page to the router and sidebar, you should be able to navigate to it by clicking the link in the sidebar. At this point it will try to render the component specified in the router, so make sure you've created that component ( and imported it in `router.js`).

The structure of the components is fairly simple, and you can see how the existing components are built by looking at the files in the `components` folder. It is basically a function that returns some html, and if you need it to be reactive you can defined a state object and use the `reactive` function from Arrow.js to make it reactive.

`sample-component.js`
```js
import { html, reactive } from "https://esm.sh/@arrow-js/core"

export function SampleCompoent() {
  const state = reactive({
    someValue: "Hello world"
  })

  return html`
    <div>
      <h1>${state.someValue}</h1>
    </div>
  `
}
```

> Notice how the returned string is a template literal, enabling you to add js expressions using the `${}` syntax.



---


# Running

### To run using docker:

```bash
docker build -t thepond .
docker run -v $(pwd):/app --rm -ti -p 8084:8084 thepond
```

### Run and debug on comma device (or computer with python)

```bash
./start.sh
```
