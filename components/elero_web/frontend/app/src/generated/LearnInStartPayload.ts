interface LearnInStartPayload {
  'type': 'learn_in_start';
  /**
   * 3-byte virtual remote/source address (hex string)
   * @example 0x17a753
   */
  'src_address': string;
  /**
   * RF channel to learn in
   * @example 5
   */
  'channel': number;
  /**
   * Empirically determined RF command byte for the remote's programming/P action.
   * @example 0x55
   */
  'programming_cmd': string;
  /**
   * Number of RF button packets per learn-in step (default 3)
   * @example 3
   */
  'packets'?: number;
  /**
   * Secondary type byte for button packets (default 0x10)
   * @example 0x10
   */
  'type2'?: string;
  /**
   * Hop count byte for button packets (default 0x00)
   * @example 0x00
   */
  'hop'?: string;
  /**
   * Learn-in session timeout in milliseconds (default 300000)
   * @example 300000
   */
  'session_timeout_ms'?: number;
}
export { LearnInStartPayload };
