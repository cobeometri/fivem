/* eslint-disable no-console */
import { injectable, inject, named } from "inversify";
import { makeAutoObservable } from "mobx";

import {
  defineService,
  ServicesContainer,
  useService,
} from "cfx/base/servicesContainer";
import {
  registerAppContribution,
  AppContribution,
} from "cfx/common/services/app/app.extensions";
import { ScopedLogger } from "cfx/common/services/log/scopedLogger";
import axios, { AxiosError } from "axios";
import {
  ArticleItem,
  ArticleResponse,
  AuthUser,
  ChangePasswordPayload,
  IRegisterPayload,
  AuthValidateResponse,
  LoginPayload,
  Notification,
  IAuthResponse
} from "./type";
import { mpMenu } from "../../mpMenu";
import { IServerView } from "cfx/common/services/servers/types";

export type ILauncherService = LauncherService;
export const ILauncherService =
  defineService<ILauncherService>("LauncherService");

export function registerLauncherService(container: ServicesContainer) {
  container.registerImpl(ILauncherService, LauncherService);
  registerAppContribution(container, LauncherService);
}

const callApi = axios.create({
  baseURL: "https://grandrp.vn/api/launcher",
  headers: {
    "Content-Type": "application/json",
  },
});

const contenApi = axios.create({
  baseURL: "https://content.grandrp.vn",
  headers: {
    "Content-Type": "application/json",
  },
});

@injectable()
class LauncherService implements AppContribution {
  @inject(ScopedLogger)
  @named("LauncherService")
  private logService: ScopedLogger;

  servers: IServerView[] = [];
  loading = false;

  constructor() {
    makeAutoObservable(this);
  }

  private serverFetchInterval: NodeJS.Timeout | null = null;

  async init() {
    this.logService.log("init");
    /* const notiTypes = ["error", "success", "info", "warning"] as const;
    setInterval(() => {
      const type = notiTypes[Math.floor(Math.random() * notiTypes.length)];
      this.addNotification({
        message: `Đây là một thông báo ${type}, asndjasknd kjasdn askj nk`,
        type,
      });
    }, 5000); */
    this.setToken(this.getToken());
    await this.me();

    this.startServerFetching();
  }

  setServers(servers: IServerView[]) {
    this.servers = servers;
  }

  get totalPlayersOnline(): number {
    return this.servers.reduce((sum, s) => sum + (s.playersCurrent ?? 0), 0);
  }

  get totalPlayersMax(): number {
    return this.servers.reduce((sum, s) => sum + (s.playersMax ?? 0), 0);
  }

  private startServerFetching() {
    this.fetchAndUpdateServers();

    this.serverFetchInterval = setInterval(() => {
      this.fetchAndUpdateServers();
    }, 30000);
  }

  private async fetchAndUpdateServers() {
    try {
      const serverList = await this.fetchServerList();
      this.setServers(serverList);
    } catch (error) {
      if (error instanceof AxiosError) {
        this.addNotification({
          message: error.response?.data?.error?.message || 'Lỗi không thể tải danh sách máy chủ',
          type: 'error',
        });
      } else {
        this.addNotification({
          message: (error as AxiosError)?.message || 'Lỗi không xác định khi tải máy chủ',
          type: 'error',
        });
      }
      this.logService.error(error as Error);
    }
  }

  destroy() {
    if (this.serverFetchInterval) {
      clearInterval(this.serverFetchInterval);
      this.serverFetchInterval = null;
    }
  }

  setLoading(loading: boolean) {
    this.loading = loading;
  }

  user: AuthUser | null = null;
  setUser(user: AuthUser | null) {
    this.user = user;
  }

  token: string | null = null;
  setToken(token: string | null) {
    this.token = token;

    if (!token) {
      return;
    }
    mpMenu.invokeNative("setToken", token);
  }

  getToken() {
    const token = localStorage.getItem("token");

    return token || null;
  }

  async register(payload: IRegisterPayload): Promise<boolean> {
    this.setLoading(true);

    try {
      await callApi.post<IAuthResponse>(
        "/auth/register",
        {
          username: payload.username,
          email: payload.email,
          password: payload.password
        }
      );

      this.addNotification({
        message: "Đăng ký tài khoản thành công!",
        type: "success",
      });
      this.addNotification({
        message: "Vui lòng kiểm tra email để tiến hành xác thực tài khoản!",
        type: "info",
        duration: 10000,
      });
      this.setLoading(false);

      return true;
    } catch (error: unknown) {
      if (error instanceof AxiosError) {
        this.addNotification({
          message: error.response?.data?.error?.message || "Lỗi không xác định",
          type: "error",
        });
      } else {
        this.addNotification({
          message: (error as AxiosError)?.message || "Lỗi không xác định",
          type: "error",
        });
      }
      this.setLoading(false);

      return false;
    }
  }

  async login(payload: LoginPayload, rememberMe: boolean): Promise<boolean> {
    this.setLoading(true);

    try {
      const response = await callApi.post<IAuthResponse>(
        "/auth/login",
        {
          identifier: payload.identifier,
          password: payload.password,
        }
      );

      this.addNotification({
        message: "Đăng nhập thành công!",
        type: "success",
      });

      if (rememberMe) {
        localStorage.setItem("token", response.data.token);
      } else {
        localStorage.removeItem("token");
      }
      this.setToken(response.data.token);
      this.setUser(response.data.user);

      this.setLoading(false);

      return true;
    } catch (error: unknown) {
      if (error instanceof AxiosError) {
        console.log(error);
        this.addNotification({
          message: error.response?.data?.message || "Lỗi không xác định",
          type: "error",
        });
      } else {
        this.addNotification({
          message: (error as AxiosError)?.message || "Lỗi không xác định",
          type: "error",
        });
      }
      this.setLoading(false);

      return false;
    }
  }

  async me() {
    if (!this.token) {
      return;
    }
    this.setLoading(true);

    try {
      const response = await callApi.get<AuthValidateResponse>("/auth/valididtoken", {
        headers: {
          Authorization: `Bearer ${this.token}`,
        },
      });

      if (response.data && response.data.id) {
        this.setUser(response.data);
      }

      this.setLoading(false);

      return response.data;
    } catch (error: unknown) {
      this.setLoading(false);
      localStorage.removeItem("token");
      this.setToken(null);

      if (error instanceof AxiosError) {
        this.addNotification({
          message: error.response?.data?.error?.message || "Lỗi không xác định",
          type: "error",
        });
      } else {
        this.addNotification({
          message: (error as AxiosError)?.message || "Lỗi không xác định",
          type: "error",
        });
      }

      return null;
    }
  }

  async changePassword(payload: ChangePasswordPayload): Promise<boolean> {
    if (!this.token) {
      this.addNotification({
        message: "Bạn cần đăng nhập để thay đổi mật khẩu!",
        type: "error",
      });

      return false;
    }

    this.setLoading(true);

    try {
      const response = await callApi.post<IAuthResponse>(
        "/auth/changerpassword",
        {
          token: this.token,
          oldPassword: payload.currentPassword,
          newPassword: payload.newPassword
        }
      );

      this.setUser(response.data.user);
      this.setLoading(false);
      this.addNotification({
        message: "Đổi mật khẩu thành công!",
        type: "success",
      });
      this.logout();

      return true;
    } catch (error: unknown) {
      this.setLoading(false);

      if (error instanceof AxiosError) {
        this.addNotification({
          message: error.response?.data?.error?.message || "Lỗi không xác định",
          type: "error",
        });
      } else {
        this.addNotification({
          message: (error as AxiosError)?.message || "Lỗi không xác định",
          type: "error",
        });
      }

      return false;
    }
  }

  async forgotPassword(email: string): Promise<boolean> {
    this.setLoading(true);

    try {
      await callApi.post("/auth/forgetpassword", { email });
      this.setLoading(false);
      this.addNotification({
        message: "Vui lòng kiểm tra Email của bạn!",
        type: "success",
      });

      return true;
    } catch (error: unknown) {
      this.setLoading(false);

      if (error instanceof AxiosError) {
        this.addNotification({
          message: error.response?.data?.error?.message || "Lỗi không xác định",
          type: "error",
        });
      } else {
        this.addNotification({
          message: (error as AxiosError)?.message || "Lỗi không xác định",
          type: "error",
        });
      }

      return false;
    }
  }

  notifications: Notification[] = [];
  addNotification = (notification: Omit<Notification, "id">) => {
    const id = crypto.randomUUID();
    this.notifications.push({
      ...notification,
      id,
    });
    setTimeout(() => {
      this.removeNotification(id);
    }, notification.duration || 5000);
  };

  removeNotification(id: string) {
    this.notifications = this.notifications.filter((n) => n.id !== id);
  }

  logout() {
    this.setUser(null);
    this.setToken(null);
    localStorage.removeItem("token");
    mpMenu.invokeNative("setToken", "");
    this.addNotification({
      message: "Đăng xuất thành công!",
      type: "success",
    });
  }

  async getArticles(): Promise<ArticleItem[]> {
    try {
      const response = await callApi.get<ArticleResponse>(
        "/news?limit=5"
      );

      return response.data.data;
    } catch (error: unknown) {
      if (error instanceof AxiosError) {
        this.addNotification({
          message: error.response?.data?.error?.message || "Lỗi không xác định",
          type: "error",
        });
      } else {
        this.addNotification({
          message: (error as AxiosError)?.message || "Lỗi không xác định",
          type: "error",
        });
      }

      return [];
    }
  }

  async fetchServerList(): Promise<IServerView[]> {
    try {
      const response = await contenApi.get<{
        status: string;
        data: IServerView[];
      }>("/server/ingress");

      if (response.data.status !== "OK") {
        this.addNotification({
          message: "Không thể lấy danh sách máy chủ",
          type: "error",
        });

        return [];
      }

      return response.data.data;
    } catch (error: unknown) {
      if (error instanceof AxiosError) {
        this.addNotification({
          message: error.response?.data?.error?.message || "Lỗi không xác định",
          type: "error",
        });
      } else {
        this.addNotification({
          message: (error as AxiosError)?.message || "Lỗi không xác định",
          type: "error",
        });
      }

      return [];
    }
  }
}

export function useLauncherService(): ILauncherService {
  return useService(ILauncherService);
}
