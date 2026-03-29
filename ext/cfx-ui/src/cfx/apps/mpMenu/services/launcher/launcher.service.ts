import axios from 'axios';

// ============================================================
// GrandRP Launcher Service
// Custom authentication, news, and server list management
// ============================================================

// API Instances
const callApi = axios.create({
  baseURL: 'https://api.grandrp.vn/api',
  headers: {
    'Content-Type': 'application/json',
  },
});

const backendApi = axios.create({
  baseURL: 'https://content.grandrp.vn',
  headers: {
    'Content-Type': 'application/json',
  },
});

// Types
export interface GrandRPUser {
  id: number;
  username: string;
  email: string;
}

export interface GrandRPArticle {
  id: number;
  title: string;
  slug: string;
  content: string;
  cover?: { url: string };
  publishedAt: string;
}

export interface GrandRPServer {
  name: string;
  address: string;
  players: number;
  maxPlayers: number;
  status: 'online' | 'offline';
}

export interface LauncherNotification {
  type: 'success' | 'error' | 'info';
  message: string;
}

// State
let currentUser: GrandRPUser | null = null;
let authToken: string | null = null;
const listeners: Array<(notification: LauncherNotification) => void> = [];

function notify(notification: LauncherNotification) {
  listeners.forEach((fn) => fn(notification));
}

function getAuthHeaders() {
  return authToken ? { Authorization: `Bearer ${authToken}` } : {};
}

// ============================================================
// Authentication
// ============================================================

export async function register(username: string, email: string, password: string): Promise<boolean> {
  try {
    await callApi.post('/auth/local/register', { username, email, password });
    notify({
      type: 'success',
      message: 'Đăng ký tài khoản thành công! Vui lòng kiểm tra email để xác thực.',
    });
    return true;
  } catch (e: any) {
    const msg = e?.response?.data?.error?.message || 'Lỗi không xác định';
    notify({ type: 'error', message: msg });
    return false;
  }
}

export async function login(identifier: string, password: string): Promise<boolean> {
  try {
    const res = await callApi.post('/auth/local', { identifier, password });
    authToken = res.data.jwt;
    currentUser = res.data.user;
    if (authToken) {
      localStorage.setItem('grandrp_token', authToken);
    }
    notify({ type: 'success', message: 'Đăng nhập thành công!' });
    return true;
  } catch (e: any) {
    const msg = e?.response?.data?.error?.message || 'Sai tên đăng nhập hoặc mật khẩu';
    notify({ type: 'error', message: msg });
    return false;
  }
}

export async function validateToken(): Promise<boolean> {
  const saved = localStorage.getItem('grandrp_token');
  if (!saved) return false;

  try {
    authToken = saved;
    const res = await callApi.get('/users/me', { headers: getAuthHeaders() });
    currentUser = res.data;
    return true;
  } catch {
    authToken = null;
    currentUser = null;
    localStorage.removeItem('grandrp_token');
    return false;
  }
}

export async function changePassword(currentPassword: string, newPassword: string): Promise<boolean> {
  if (!authToken) {
    notify({ type: 'error', message: 'Bạn cần đăng nhập để thay đổi mật khẩu!' });
    return false;
  }

  try {
    await callApi.post('/auth/change-password', {
      currentPassword,
      password: newPassword,
      passwordConfirmation: newPassword,
    }, { headers: getAuthHeaders() });
    notify({ type: 'success', message: 'Đổi mật khẩu thành công!' });
    return true;
  } catch (e: any) {
    const msg = e?.response?.data?.error?.message || 'Lỗi không xác định';
    notify({ type: 'error', message: msg });
    return false;
  }
}

export async function forgotPassword(email: string): Promise<boolean> {
  try {
    await callApi.post('/auth/forgot-password', { email });
    notify({ type: 'success', message: 'Đã gửi email khôi phục mật khẩu!' });
    return true;
  } catch (e: any) {
    const msg = e?.response?.data?.error?.message || 'Lỗi không xác định';
    notify({ type: 'error', message: msg });
    return false;
  }
}

export function logout() {
  authToken = null;
  currentUser = null;
  localStorage.removeItem('grandrp_token');
  notify({ type: 'success', message: 'Đăng xuất thành công!' });
}

export function getCurrentUser(): GrandRPUser | null {
  return currentUser;
}

export function onNotification(fn: (notification: LauncherNotification) => void) {
  listeners.push(fn);
  return () => {
    const idx = listeners.indexOf(fn);
    if (idx >= 0) listeners.splice(idx, 1);
  };
}

// ============================================================
// News / Articles
// ============================================================

export async function fetchArticles(): Promise<GrandRPArticle[]> {
  try {
    const res = await callApi.get('/articles?populate=*&sort[0]=publishedAt:desc&pagination[limit]=5');
    return res.data.data || [];
  } catch {
    return [];
  }
}

// ============================================================
// Server List
// ============================================================

export async function fetchServers(): Promise<GrandRPServer[]> {
  try {
    const res = await backendApi.get('/server/ingress');
    return res.data || [];
  } catch {
    notify({ type: 'error', message: 'Không thể lấy danh sách máy chủ' });
    return [];
  }
}

// ============================================================
// Social Links
// ============================================================

export async function fetchSocialLinks(): Promise<Record<string, string>> {
  try {
    const res = await callApi.get('/global');
    return res.data?.data?.socials || {};
  } catch {
    return {};
  }
}
