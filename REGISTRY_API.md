# NextViper Registry API Error Specification

All public HTTP endpoints exposed by the NextViper package registry (`https://nextviper.nuratix.com/api`) return consistent, structured JSON responses on failure.

---

## Standard Error Response Schema

```json
{
  "error": {
    "code": "PACKAGE_NOT_FOUND",
    "message": "Package `example` was not found in registry.",
    "documentation": "https://nextviper.nuratix.com/docs/errors/package-not-found"
  }
}
```

---

## Error Codes Matrix

| HTTP Status | API Code | Description | Documentation URL |
| :--- | :--- | :--- | :--- |
| `400 Bad Request` | `INVALID_ARGUMENT` | Malformed payload, invalid version string, or schema violation | `https://nextviper.nuratix.com/docs/errors/invalid-argument` |
| `401 Unauthorized` | `AUTHENTICATION_REQUIRED` | Missing or invalid API bearer token | `https://nextviper.nuratix.com/docs/authentication` |
| `403 Forbidden` | `PERMISSION_DENIED` | Insufficient permissions to publish or modify package | `https://nextviper.nuratix.com/docs/publishing` |
| `404 Not Found` | `PACKAGE_NOT_FOUND` | The requested package or version does not exist | `https://nextviper.nuratix.com/docs/errors/package-not-found` |
| `409 Conflict` | `VERSION_EXISTS` | The specified package version has already been published | `https://nextviper.nuratix.com/docs/publishing` |
| `429 Too Many Requests` | `RATE_LIMIT_EXCEEDED` | Request rate limit exceeded | `https://nextviper.nuratix.com/docs/api` |
| `500 Server Error` | `INTERNAL_SERVER_ERROR` | Internal server error (internal details suppressed) | `https://nextviper.nuratix.com/docs/errors/compiler-error` |

---

## Production Security Guarantees

- **No Secrets**: API error responses never expose database credentials, connection strings, SQL queries, or internal stack traces.
- **No 404s in Documentation Links**: Every `"documentation"` URL in API responses resolves to a live documentation page.
