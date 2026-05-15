import {LearnInState} from './LearnInState';
/**
 * Thin snapshot of the backend learn-in session.
 */
interface LearnInStateData {
  /**
   * Backend learn-in session state.
   */
  'state': LearnInState;
  /**
   * True while a learn-in session is logically in progress.
   */
  'active': boolean;
  /**
   * True while RF transmissions for the current learn-in step are pending.
   */
  'busy': boolean;
  /**
   * Virtual remote source address for the current session, if any.
   * @example 0x17a753
   */
  'src_address'?: string;
  /**
   * RF channel for the current session, if any.
   * @example 5
   */
  'channel'?: number;
  /**
   * RF command byte used for the programming/P action, if any.
   * @example 0x55
   */
  'programming_cmd'?: string;
}
export { LearnInStateData };