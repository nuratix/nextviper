# Benchmark 01: Arithmetic - Prime calculation and numeric series in Python
def is_prime(n):
    if n <= 1:
        return False
    i = 2
    while i * i <= n:
        if n % i == 0:
            return False
        i += 1
    return True

def count_primes(limit):
    count = 0
    n = 2
    while n <= limit:
        if is_prime(n):
            count += 1
        n += 1
    return count

total_primes = count_primes(5000)
print(total_primes)
