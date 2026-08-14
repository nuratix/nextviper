# Benchmark 03: Function Calls - Deep recursive tree calls in Python
def fib(n):
    if n <= 1:
        return n
    return fib(n - 1) + fib(n - 2)

res = fib(25)
print(res)
