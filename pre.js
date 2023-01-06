function fileLoad(event, filename) {
    var file = event.target.files[0];
    var reader = new FileReader();
    reader.onloadend = function(event) {
      if(event.target.readyState == FileReader.DONE)
        FS.writeFile(filename, new Uint8Array(event.target.result), {encoding: 'binary'});
    };
    reader.readAsArrayBuffer(file);
}

let fileInput = document.createElement("input");
fileInput.setAttribute("type", "file");
fileInput.onchange = () => { fileLoad(event, "executable"); };
document.body.appendChild(fileInput);

let button = document.createElement("button");
button.innerText = "Start";
button.onclick = () => { Module.callMain(["executable"]); };
document.body.appendChild(button);

// Also show stderr in the page
Module.printErr = (t) => { Module.print(t); };
