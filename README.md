# josh

A one-shot JSON parser.

## What does that mean?

It means that `josh` will try to parse as little JSON as possible, using as
little memory as possible, all while doing it as fast as possible. `josh`
is ideal when you want to extract portions of JSON and do little to no
modifications, and know the structure of your JSON ahead of time.

## Example

```c
int main(void) {
    // define our stringified JSON
    const char *json = "{\"hello\": [\"world\"]}";
    
    // get a pointer to the first element in the "hello" key
    size_t len = 0;
    const char *pos = josh_extract(json, ".hello[0]", &len);
    
    // print the portion of the JSON containing the key from above
    printf("got: %.*s\n", len, pos);
}
```

Compiling and running gives us the following output:

```
got: "world"
```

`josh` uses a syntax similar to [`jq`](https://github.com/stedolan/jq) for extracting
values. In the above example we essentially get a string view to the portion of our
JSON which contains the key we asked for, all without any calls to `malloc()`.
