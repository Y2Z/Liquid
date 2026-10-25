# Sample Liquid apps

Copy `.liquid` files from this directory into Liquid's `apps` directory, and then run them using `liquid <app-name>`:

    mkdir -p ~/.liquid/apps/
    cp -n *.liquid ~/.liquid/apps/
    liquid hackernews

Liquid's `apps` directory is located at:

 - GNU/Linux, BSD, and others: `~/.liquid/apps/`
 - macOS: `~/Library/Application Support/Liquid/apps/`
 - Windows: `%LOCALAPPDATA%\Liquid\apps\`
