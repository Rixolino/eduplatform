// API Client - Comunicazione con il backend
class APIClient {
  constructor(baseURL = "") {
    this.baseURL = baseURL;
    this.token = localStorage.getItem("auth_token");
  }

  setToken(token) {
    this.token = token;
    localStorage.setItem("auth_token", token);
  }

  getToken() {
    return this.token;
  }

  clearToken() {
    this.token = null;
    localStorage.removeItem("auth_token");
  }

  async request(method, endpoint, data = null) {
    const url = `${this.baseURL}${endpoint}`;
    const options = {
      method: method,
      headers: {
        "Content-Type": "application/json",
        "Access-Control-Allow-Origin": "*",
      },
    };

    if (this.token) {
      options.headers["Authorization"] = `Bearer ${this.token}`;
    }

    if (data) {
      options.body = JSON.stringify(data);
    }

    try {
      const response = await fetch(url, options);

      if (!response.ok) {
        let error = { message: `HTTP ${response.status}` };
        try {
          // 1. Try to parse JSON body for structured API errors (preferred)
          const jsonErrorBody = await response.json();
          error = {
            ...jsonErrorBody,
            message:
              jsonErrorBody.error ||
              jsonErrorBody.message ||
              `API Error (${response.status})`,
          };
        } catch (e) {
          // 2. If JSON parsing fails, read the text body as a fallback
          const textError = await response.text();
          error = {
            message: `HTTP ${response.status}: ${response.statusText || "Unknown error"}`,
          };
          console.warn("Could not parse structured API error response:", e);
          // Optional: Include relevant part of the text body if it looks like a simple string description
          if (textError && !textError.includes("Content-Type")) {
            error.details = `Raw body excerpt: ${textError.substring(0, 150)}...`;
          }
        }
        throw new Error(error.message);
      }

      return await response.json();
    } catch (error) {
      console.error("API Error:", error);
      throw error;
    }
  }

  // User endpoints
  async register(userData) {
    return this.request("POST", "/api/users/register", userData);
  }

  async login(credentials) {
    const response = await this.request(
      "POST",
      "/api/users/login",
      credentials,
    );
    if (response.token) {
      this.setToken(response.token);
    }
    return response;
  }

  async logout() {
    this.clearToken();
    return true;
  }

  async getUserProfile() {
    return this.request("GET", "/api/users/profile");
  }

  // Course endpoints
  async getCourses(filters = {}) {
    let query = "/api/courses";
    const params = new URLSearchParams();

    if (filters.category) params.append("category", filters.category);
    if (filters.difficulty) params.append("difficulty", filters.difficulty);
    if (filters.search) params.append("search", filters.search);

    if (params.toString()) {
      query += "?" + params.toString();
    }

    return this.request("GET", query);
  }

  async getCourseTasks(courseId) {
    return this.request("GET", `/api/tasks?course_id=${courseId}`);
  }

  async getCourse(courseId) {
    return this.request("GET", `/api/courses/${courseId}`);
  }

  async createCourse(courseData) {
    return this.request("POST", "/api/courses", courseData);
  }

  async updateCourse(courseId, courseData) {
    return this.request("PUT", `/api/courses/${courseId}`, courseData);
  }

  async deleteCourse(courseId) {
    return this.request("DELETE", `/api/courses/${courseId}`);
  }

  // Lesson endpoints
  async getLessons(courseId) {
    return this.request("GET", `/api/courses/${courseId}/lessons`);
  }

  async getLesson(courseId, lessonId) {
    return this.request("GET", `/api/courses/${courseId}/lessons/${lessonId}`);
  }

  async createLesson(courseId, lessonData) {
    return this.request("POST", `/api/courses/${courseId}/lessons`, lessonData);
  }

  async updateLesson(courseId, lessonId, lessonData) {
    return this.request(
      "PUT",
      `/api/courses/${courseId}/lessons/${lessonId}`,
      lessonData,
    );
  }

  async deleteLesson(courseId, lessonId) {
    return this.request(
      "DELETE",
      `/api/courses/${courseId}/lessons/${lessonId}`,
    );
  }

  // Enrollment endpoints
  async enrollCourse(courseId) {
    return this.request("POST", "/api/enrollments", { course_id: courseId });
  }

  async getEnrollments() {
    return this.request("GET", "/api/enrollments");
  }

  async getEnrollment(enrollmentId) {
    return this.request("GET", `/api/enrollments/${enrollmentId}`);
  }

  async updateProgress(enrollmentId, lessonId) {
    return this.request("PUT", `/api/enrollments/${enrollmentId}/progress`, {
      lesson_id: lessonId,
    });
  }

  async completeCourse(enrollmentId) {
    return this.request("PUT", `/api/enrollments/${enrollmentId}/complete`);
  }

  // Feedback endpoints
  async submitFeedback(enrollmentId, feedbackData) {
    return this.request(
      "POST",
      `/api/enrollments/${enrollmentId}/feedback`,
      feedbackData,
    );
  }

  async getFeedback(courseId) {
    return this.request("GET", `/api/courses/${courseId}/feedback`);
  }

  // Statistics endpoints
  async getPopularCourses() {
    return this.request("GET", "/api/statistics/popular-courses");
  }

  async getCompletionRate() {
    return this.request("GET", "/api/statistics/completion-rate");
  }

  async getEnrollmentStats() {
    return this.request("GET", "/api/statistics/enrollments");
  }

  // Paths endpoints
  async getPaths() {
    return this.request("GET", "/api/paths");
  }

  async getPathDetails(pathId) {
    return this.request("GET", `/api/paths/${pathId}`);
  }

  // Teacher endpoints
  async getTeacherStudents() {
    return this.request("GET", "/api/teacher/students");
  }

  async createTask(taskData) {
    return this.request("POST", "/api/tasks", taskData);
  }

  async updateTask(taskId, taskData) {
    return this.request("PUT", `/api/tasks/${taskId}`, taskData);
  }

  async deleteTask(taskId) {
    return this.request("DELETE", `/api/tasks/${taskId}`);
  }

  async getTaskDetails(taskId) {
    return this.request("GET", `/api/tasks?id=${taskId}`);
  }

  async uploadFile(file) {
    const formData = new FormData();
    formData.append("file", file);

    const url = `${this.baseURL}/api/uploads`;
    const token = this.getToken();

    const response = await fetch(url, {
      method: "POST",
      headers: token ? { Authorization: `Bearer ${token}` } : {},
      body: formData,
    });

    if (!response.ok) throw new Error("Upload failed");
    return await response.json();
  }

  async submitSubmission(submissionData) {
    return this.request("POST", "/api/submissions", submissionData);
  }

  // Study Buddy
  async getCourseBuddies(courseId) {
    return this.request("GET", `/api/courses/${courseId}/buddies`);
  }

  // Skills
  async getUserSkills() {
    return this.request("GET", "/api/users/skills");
  }

  // Health check
  async healthCheck() {
    return this.request("GET", "/api/health");
  }
}

// Esportare l'istanza globale
const api = new APIClient();
