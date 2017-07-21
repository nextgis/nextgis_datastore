# Intro
This is the coding style

# Main rules
1. All includes sort alphabetically (may be grouped, i.e. stl, gdal, this project)
2. Don't use using namespace for std
3. Initialization of float/double should be a real number (i.e. float a = 1.0)
4. Getters, setters or one line functions put in header. Other in sources.
5. If brackets ('{' and '}') use in function in one place, should use in another:

For example
```
void test(int type)
{
    if(type == 1) {
        int x = 1
    }
    else <-- Wrong - need {
        int y = 1;
}
```

6. The pointer asterisk and reference ampersand signs in the variable's declaration should be at left (not right), close to the type name. In the dereferencing operation, the pointer asterisk sign must be on the right, close to the variable name. In the operation of taking the address, the  ampersand signs must be on the right, close to the variable name too.

For example:
```
const char& m = 'g';
const char* name;
char** list;
Dataset* parentDS;
Dataset* const dataset;
but:
const char n = *name;
const char* pm = &m;
```

7. Variable name and functions - use camel style from lower case
8. Class name - from upper case
9. In getters try not use get

For example:
```
size_t getFeatureCount() <-- Wrong
size_t featureCount() <-- Right
```

10. For smart pointers use Ptr and UPtr and WPtr postfix

# TBD

# Links

* [Google coding guide](https://google.github.io/styleguide/cppguide.html)
