// Errors that carry the HTTP status and a stable machine-readable code.
export class ApiError extends Error {
  constructor(status, code, message) {
    super(message);
    this.name = 'ApiError';
    this.status = status;
    this.code = code;
  }
}

export class ValidationError extends ApiError {
  constructor(message) {
    super(400, 'invalid_request', message);
    this.name = 'ValidationError';
  }
}

// The runner could not take the job right now.
export class BusyError extends Error {
  constructor(message = 'the server is busy') {
    super(message);
    this.name = 'BusyError';
  }
}
