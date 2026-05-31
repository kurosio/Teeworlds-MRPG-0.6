import { sha256 as sha256Lib } from 'js-sha256';

export async function sha256(message: string): Promise<string> {
  return sha256Lib(message);
}