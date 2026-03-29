export interface IRegisterPayload {
  username: string;
  email: string;
  password: string;
  name: string;
}

export interface AuthUser {
  id: string;
  license: string;
  role: string;
  username: string;
  email: string;
  emailVerificationExpires: string | null;
  emailVerified: boolean;
  createdAt : string;
  updatedAt: string;
}

export interface IAuthResponse {
  token: string;
  user: AuthUser;
}

export type AuthValidateResponse = AuthUser;

export interface ErrorResponse {
  data: null;
  error: ErrorDetail;
}

export interface ErrorDetail {
  status: number;
  name: string;
  message: string;
  details: Record<string, any>;
}

export interface Notification {
  id: string;
  type: "success" | "error" | "info" | "warning";
  message: string;
  duration?: number; // duration in milliseconds
}

export interface LoginPayload {
  identifier: string; // can be username or email
  password: string;
}

export interface ChangePasswordPayload {
  currentPassword: string;
  newPassword: string;
}

// Image Formats
export interface ImageFormat {
  name: string;
  hash: string;
  ext: string;
  mime: string;
  path: string | null;
  width: number;
  height: number;
  size: number;
  sizeInBytes: number;
  url: string;
}

// Image Asset
export interface ImageAsset {
  id: number;
  documentId: string;
  name: string;
  alternativeText: string | null;
  caption: string | null;
  width: number;
  height: number;
  formats: {
    thumbnail: ImageFormat;
    small: ImageFormat;
    medium: ImageFormat;
    large: ImageFormat;
  };
  hash: string;
  ext: string;
  mime: string;
  size: number;
  url: string;
  previewUrl: string | null;
  provider: string;
  provider_metadata: any;
  createdAt: string;
  updatedAt: string;
  publishedAt: string;
  locale: string | null;
}

// Rich Text Block
export interface RichTextBlock {
  __component: "shared.rich-text";
  id: number;
  body: string;
}

// Media Block
export interface MediaBlock {
  __component: "shared.media";
  id: number;
}

// Quote Block
export interface QuoteBlock {
  __component: "shared.quote";
  id: number;
  title: string;
  body: string;
}

// Slider Block
export interface SliderBlock {
  __component: "shared.slider";
  id: number;
}

// Union Type cho tất cả block component
export type ContentBlock =
  | RichTextBlock
  | MediaBlock
  | QuoteBlock
  | SliderBlock;

// Announce Item
export interface ArticleItem {
  id: number;
  documentId: string;
  title: string;
  description: string;
  slug: string;
  type: string;
  createdAt: string;
  updatedAt: string;
  publishedAt: string;
  locale: string | null;
  cover: ImageAsset;
  author: any; // Nếu cần rõ, hãy thay thế bằng interface cụ thể
  category: any;
  blocks: ContentBlock[];
  localizations: any[];
}

// Pagination Metadata
export interface PaginationMeta {
  start: number;
  limit: number;
  total: number;
}

// Tổng thể Response
export interface ArticleResponse {
  data: ArticleItem[];
  meta: {
    pagination: PaginationMeta;
  };
}

export interface GlobalData {
  id: number;
  documentId: string;
  launcher_name: string;
  launcher_description: string;
  forum_url: string;
  fanpage_url: string;
  discord_url: string;
  youtube_url: string;
  tiktok_url: string;
  createdAt: string;
  updatedAt: string;
  publishedAt: string;
  locale: string | null;
  launcher_youtube_link: string | null;
}

export interface PlayerInfoResponse {
  success: boolean;
  message: string;
  data: {
    identifier: string;
    firstname: string;
    lastname: string;
    fullname: string;
  };
}

export type GlobalResponse = GlobalData;