# How to Generate Documentation

These are the dependencies that are required to generate documentation in Ubuntu

```bash
# Install doxygen
sudo apt-get install -y doxygen

# Install pip packages required by m.css
pip3 install jinja2 Pygments
sudo apt install \
    texlive-base \
    texlive-latex-extra \
    texlive-fonts-extra \
    texlive-fonts-recommended
```


After that is only a matter of executing the following command from the repo's root folder
```bash
./ThirdParty/m.css/documentation/doxygen.py Doxyfile
```



## Results

You can see results in the **doc/html/index.html** file or in the online doc here

