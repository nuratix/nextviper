# Benchmark 08: Data Processing - Matrix multiplication, ReLU activation, and sum in Python
def matmul(A, B, n):
    C = [[0.0] * n for _ in range(n)]
    for i in range(n):
        for k in range(n):
            for j in range(n):
                C[i][j] += A[i][k] * B[k][j]
    return C

n = 60
A = [[1.0] * n for _ in range(n)]
B = [[1.0] * n for _ in range(n)]

C = matmul(A, B, n)

total = 0.0
for row in C:
    for val in row:
        if val > 0:
            total += val

print(total)
