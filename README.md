# ruffell.nz - Homepage and Blog

This is my personal blog, and it is a nice static site generated by Jekyll.

The theme is [Hydeout](https://github.com/fongandrew/hydeout)

To install / test:

## Install Jekyll

Install the Jekyll ruby packages:

```
sudo apt-get install ruby-full build-essential zlib1g-dev
```

Set ruby gems to be installed in a hidden directory :

```
echo '# Install Ruby Gems to ~/.gems' >> ~/.bashrc
echo 'export GEM_HOME="$HOME/.gems"' >> ~/.bashrc
echo 'export PATH="$HOME/.gems/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

Install Jekyll:

```
gem install jekyll bundler
```

Pull down website gem dependences:

```
bundle install
```

Launch the development site:

```
bundle exec jekyll serve
```

The site will then be served locally on localhost:4000.
