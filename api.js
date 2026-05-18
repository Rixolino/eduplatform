// API Client - Comunicazione con il backend
class APIClient {
    constructor(baseURL = 'http://localhost:3000') {
        this.baseURL = baseURL;
        this.token = localStorage.getItem('auth_token');
    }

    setToken(token) {
        this.token = token;
        localStorage.setItem('auth_token', token);
    }

    getToken() {
        return this.token;
    }

    clearToken() {
        this.token = null;
        localStorage.removeItem('auth_token');
    }

    async request(method, endpoint, data = null) {
        const url = `${this.baseURL}${endpoint}`;
        const options = {
            method: method,
            headers: {
                'Content-Type': 'application/json',
                'Access-Control-Allow-Origin': '*'
            }
        };

        if (this.token) {
            options.headers['Authorization'] = `Bearer ${this.token}`;
        }

        if (data) {
            options.body = JSON.stringify(data);
        }

        try {
            const response = await fetch(url, options);
            
            if (!response.ok) {
                const error = await response.json().catch(() => ({ error: response.statusText }));
                throw new Error(error.error || `HTTP ${response.status}`);
            }

            return await response.json();
        } catch (error) {
            console.error('API Error:', error);
            throw error;
        }
    }

    // User endpoints
    async register(userData) {
        return this.request('POST', '/api/users/register', userData);
    }

    async login(credentials) {
        const response = await this.request('POST', '/api/users/login', credentials);
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
        return this.request('GET', '/api/users/profile');
    }

    // Course endpoints
    async getCourses(filters = {}) {
        let query = '/api/courses';
        const params = new URLSearchParams();
        
        if (filters.category) params.append('category', filters.category);
        if (filters.difficulty) params.append('difficulty', filters.difficulty);
        if (filters.search) params.append('search', filters.search);
        
        if (params.toString()) {
            query += '?' + params.toString();
        }
        
        return this.request('GET', query);
    }

    async getCourse(courseId) {
        return this.request('GET', `/api/courses/${courseId}`);
    }

    async createCourse(courseData) {
        return this.request('POST', '/api/courses', courseData);
    }

    async updateCourse(courseId, courseData) {
        return this.request('PUT', `/api/courses/${courseId}`, courseData);
    }

    async deleteCourse(courseId) {
        return this.request('DELETE', `/api/courses/${courseId}`);
    }

    // Lesson endpoints
    async getLessons(courseId) {
        return this.request('GET', `/api/courses/${courseId}/lessons`);
    }

    async getLesson(courseId, lessonId) {
        return this.request('GET', `/api/courses/${courseId}/lessons/${lessonId}`);
    }

    async createLesson(courseId, lessonData) {
        return this.request('POST', `/api/courses/${courseId}/lessons`, lessonData);
    }

    async updateLesson(courseId, lessonId, lessonData) {
        return this.request('PUT', `/api/courses/${courseId}/lessons/${lessonId}`, lessonData);
    }

    async deleteLesson(courseId, lessonId) {
        return this.request('DELETE', `/api/courses/${courseId}/lessons/${lessonId}`);
    }

    // Enrollment endpoints
    async enrollCourse(courseId) {
        return this.request('POST', '/api/enrollments', { course_id: courseId });
    }

    async getEnrollments() {
        return this.request('GET', '/api/enrollments');
    }

    async getEnrollment(enrollmentId) {
        return this.request('GET', `/api/enrollments/${enrollmentId}`);
    }

    async updateProgress(enrollmentId, lessonId) {
        return this.request('PUT', `/api/enrollments/${enrollmentId}/progress`, { lesson_id: lessonId });
    }

    async completeCourse(enrollmentId) {
        return this.request('PUT', `/api/enrollments/${enrollmentId}/complete`);
    }

    // Feedback endpoints
    async submitFeedback(enrollmentId, feedbackData) {
        return this.request('POST', `/api/enrollments/${enrollmentId}/feedback`, feedbackData);
    }

    async getFeedback(courseId) {
        return this.request('GET', `/api/courses/${courseId}/feedback`);
    }

    // Statistics endpoints
    async getPopularCourses() {
        return this.request('GET', '/api/statistics/popular-courses');
    }

    async getCompletionRate() {
        return this.request('GET', '/api/statistics/completion-rate');
    }

    async getEnrollmentStats() {
        return this.request('GET', '/api/statistics/enrollments');
    }

    // Health check
    async healthCheck() {
        return this.request('GET', '/api/health');
    }
}

// Esportare l'istanza globale
const api = new APIClient();
