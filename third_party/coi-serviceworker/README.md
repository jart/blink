# coi-serviceworker 
![npm](https://img.shields.io/npm/v/coi-serviceworker) ![size](https://img.shields.io/github/size/gzuidhof/coi-serviceworker/coi-serviceworker.min.js)

Cross-origin isolation ([COOP and COEP](https://web.dev/coop-coep/)) through a service worker for situations in which you can't control the headers (e.g. GH pages).


## Usage

1. Download `coi-serviceworker.js` (or `coi-serviceworker.min.js`).
2. Put it next to your index file (or in any folder above it)
3. Add to your HTML file:
    ```html
    <script src="coi-serviceworker.js"></script>
    ```

This script will reload the page on the user's first load to magically add the required COOP and COEP headers in a service worker.

**Rules**:
* It must be in a separate file, you can't bundle it along with your app. 
* It can't be loaded from a CDN: it must be served from your own origin.
* Your page will still need to be either served from HTTPS, or served from localhost.


### Extra credits: download from NPM

You can install this package from npm:
```
npm i --save coi-serviceworker
```

You will still have to tell your bundler to put the file alongside your bundle. Something like this will do the trick:

```shell
cp node_modules/coi-serviceworker/coi-serviceworker.js dist/
```

## Customization
You can customize the behavior by defining a variable `coi` in the global scope (i.e. on the `window` object):

```javascript
window.coi = {
    // // A function that is run to decide whether to register the SW or not.
    // You could for instance make this return a value based on whether you actually need to be cross origin isolated or not.
    shouldRegister: () => true,
    // If this function returns true, any existing service worker will be deregistered (and nothing else will happen).
    shouldDeregister: () => false,
    // A function that is run to decide whether to use "Cross-Origin-Embedder-Policy: credentialless" or not.
    // See https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Cross-Origin-Embedder-Policy#browser_compatibility
    coepCredentialless: () => !(navigator.userAgent.indexOf("CriOS") > -1 || !window.chrome),
    // Override this if you want to prompt the user and do reload at your own leisure. Maybe show the user a message saying:
    // "Click OK to refresh the page to enable <...>"
    doReload: () => window.location.reload(),
    // Set to true if you don't want coi to log anything to the console.
    quiet: false
}
```

Library and idea based on @stefnotch's [blog post](https://dev.to/stefnotch/enabling-coop-coep-without-touching-the-server-2d3n).

## License
MIT

![Carp or Koi Artwork](https://i.imgur.com/HVyWe6T.jpeg)
> Carp or Koi (1926) by Ohara Koson. Original from the Los Angeles County Museum of Art. Public Domain CC0 image.
