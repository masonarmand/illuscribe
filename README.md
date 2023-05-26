<img src='example-image.png' width="600">

# illuscribe
Present slideshows from plaintext files.
Illuscribe is a simple program written in C using the Xlib and stb_image libraries. It parses plaintext files and renders them as presentations.

For an example of what a slideshow may look like, view the `example/` folder  
(credit to https://www.vangoghmuseum.nl for the Van Gogh painting scans)

## Keybinds
- Go to previous slide 
    - `Arrow Left`
    - `Right Mouse Button`
    - `Scroll Down`  
- Go to next slide
    - `Arrow Right`
    - `Space`
    - `Enter`
    - `Left Mouse Button`
    - `Scroll Up`  
- Toggle fullscreen
    - `Key F`  
- Resize back to original window size
    - `Key E`  
- Quit
    - `Escape`
    - `Key Q`  


## Syntax/Commands
Here are the basic commands/functions:
- `slide: name` - Declares a slide. Slides contain boxes.
    - name: string
- `box: name, <stack-direction>, <text-alignment>` - Declares a box. Boxes can contain text and images.
    - name: string
    - stack-direction: `stack-vertical`, `stack-horizontal`
    - text-alignment: `align-left`, `align-center`, `align-right`
- `define: name` - Add text or images inside of a define block, `name` specifies which box to add to.
    - name: string
- `text: <size>, content` - Add text to a box.
    - size: `huge`, `title`, `normal`, `small`
    - content: string
- `image: filename` - Add an image with the specified filename.
    - filename: string
- `end` - End a slide or define block.
- `template: name` - Exact same as defining a slide, except it will not be rendered.
    - name: string
- `uses: name` - Include a slide or template in another slide. You can then define boxes that are in those slides/templates.
    - name: string

Example of all the above commands in use:
```
template: "my-template"
    box: "title-box", stack-vertical, align-center
    box: "content-box", stack-horizontal, align-left
end

slide: "slide1"
    uses: "my-template"

    define: "title-box"
        text: title, "Hello World!"
    end

    define: "content-box"
        text: normal, "This is some text."
        image: "yourimage.png"
    end
end
```
Note: you don't need to use templates, the above code will render the same as this code below:
```
slide: "slide1"
    box: "title-box", stack-vertical, align-center
    box: "content-box", stack-horizontal, align-left

    define: "title-box"
        text: title, "Hello World!"
    end

    define: "content-box"
        text: normal, "This is some text."
        image: "yourimage.png"
    end
end
```
You can also use `uses:` on slides, not just templates, for including full slides inside other slides.
## Usage
```
illuscribe <path-to-your-slideshow-file>
```
By default Illuscribe runs at the same aspect ratio as the monitor it is launched on. If you want a different aspect ratio, you can specify the initial window dimensions like so:
```
illuscribe <path-to-you-slideshow-file> <window-width> <window-height>
```
## Installation
Install the required dependencies:
```
sudo apt update
sudo apt install libxrender-dev libx11-dev libxft-dev
```
Clone the repository and compile:
```
git clone https://github.com/masonarmand/illuscribe.git
cd illuscribe
sudo make install
```
Now you can run `illuscribe` on any slideshow file.
