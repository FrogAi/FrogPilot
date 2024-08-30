import { html, reactive } from "https://esm.sh/@arrow-js/core"

function Testcomponent() {
  let state = reactive({ 
    count: 0,
    foo: false
   })

  //random 1-10
  const random = Math.floor(Math.random() * 10) + 1

  async function fetchSomething() {
    //timeout
    await new Promise((resolve) => setTimeout(() => {
      state.count++
      state.foo = true
      resolve()
    }, 2000))
  }

  fetchSomething()

  console.log("Random number", random)
  return html`
    <div>
      <h1>${() => state.count}</h1>
      <button @click="${() => state.count++}">Increment</button>
    </div>
  `
}


html`
  <div style="color:white">
    <h1>Playground</h1>
    <p>Playground for testing out new features</p>
    ${() => Testcomponent()}
  </div>
`(document.getElementById("app"))


